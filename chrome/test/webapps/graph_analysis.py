#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Graph analysis functions for the testing framework.
"""

import logging
from typing import Dict, List, Set, Tuple

from classes import ActionCoverage
from classes import Action
from classes import ActionNode
from classes import CoverageTest
from classes import PartialCoverageAddition


def CreateFullCoverageActionGraph(root_node: ActionNode,
                                  tests: List[CoverageTest]) -> ActionNode:
    assert isinstance(root_node, ActionNode)
    assert isinstance(tests, list)
    for test in tests:
        assert isinstance(test, CoverageTest)
        parent = root_node
        for action in test.actions:
            assert isinstance(action, Action)
            assert parent is not None
            if action.is_state_check:
                parent.AddStateCheckAction(action, ActionCoverage.FULL)
            else:
                node = None
                if action.name in parent.children:
                    node = parent.children[action.name]
                else:
                    node = ActionNode(action)
                    parent.AddChild(node)
                node.coverage = ActionCoverage.FULL
                node.full_coverage_tests.add(test)
                parent = node


def AddPartialPaths(root_node: ActionNode,
                    partial_paths: List[PartialCoverageAddition]) -> None:
    assert isinstance(root_node, ActionNode)
    assert isinstance(partial_paths, list)

    partial_path_existence = set()
    for partial_path in partial_paths:
        partial_path_existence.add(partial_path.input_actions[0].name)

    # Returns the node that is the result of following the given action list
    # in the graph. Also accumulates all fully covered and partially covered
    # in the path. These tests are what the new path will partially cover.
    def FollowGraph(start: ActionNode, action_list: List[ActionNode]
                    ) -> Tuple[ActionNode, Set[Action]]:
        assert isinstance(start, ActionNode)
        assert len(action_list) > 0
        curr_node = start
        tests_to_partial_cover = curr_node.full_coverage_tests.copy()
        tests_to_partial_cover.update(curr_node.partial_coverage_tests)
        for action in action_list:
            assert isinstance(action, Action)
            if action.name in curr_node.children:
                curr_node = curr_node.children[action.name]
                tests_to_partial_cover.update(curr_node.full_coverage_tests)
                tests_to_partial_cover.update(curr_node.partial_coverage_tests)
            elif action.name not in curr_node.state_check_actions:
                return (None, None)
        return (curr_node, tests_to_partial_cover)

    def MergeStateCheckCoverage(
            a_state_check_coverage: Dict[str, ActionCoverage],
            b_state_check_coverage: Dict[str, ActionCoverage]
    ) -> Dict[str, ActionCoverage]:
        def MergePartialEntry(coverage_a: ActionCoverage,
                              coverage_b: ActionCoverage):
            if (coverage_a == ActionCoverage.FULL
                    or coverage_b == ActionCoverage.FULL):
                return ActionCoverage.FULL
            return ActionCoverage.PARTIAL

        # Start with symmetric difference
        result = ({
            k: a_state_check_coverage.get(k, b_state_check_coverage.get(k))
            for k in (a_state_check_coverage.keys()
                      ^ b_state_check_coverage.keys())
        })
        # Update with `MergePartialEntry()` applied to the intersection
        result.update({
            k: MergePartialEntry(a_state_check_coverage[k],
                                 b_state_check_coverage[k])
            for k in (a_state_check_coverage.keys()
                      & b_state_check_coverage.keys())
        })
        return result

    def MergeNodes(node_a: ActionNode, node_b: ActionNode) -> ActionNode:
        assert not node_a.dead
        assert not node_b.dead
        if node_a == node_b:
            return node_a
        assert node_a.action.name == node_b.action.name
        logging.info("Merging nodes with paths:\n{" +
                     node_a.GetGraphPathStr() + "},\n{" +
                     node_b.GetGraphPathStr() + "}")
        new_node = ActionNode(node_a.action)
        new_node.coverage = (ActionCoverage.FULL
                             if node_a.coverage == ActionCoverage.FULL
                             or node_b.coverage == ActionCoverage.FULL else
                             ActionCoverage.PARTIAL)
        new_node.state_check_actions = node_a.state_check_actions
        new_node.state_check_actions.update(node_b.state_check_actions)
        new_node.state_check_actions_coverage = MergeStateCheckCoverage(
            node_a.state_check_actions_coverage,
            node_b.state_check_actions_coverage)
        new_node.full_coverage_tests = node_a.full_coverage_tests
        new_node.full_coverage_tests.update(node_b.full_coverage_tests)
        new_node.partial_coverage_tests = node_a.partial_coverage_tests
        new_node.partial_coverage_tests.update(node_b.partial_coverage_tests)
        children_a = node_a.children.copy()
        children_b = node_b.children.copy()
        parents_a = node_a.parents.copy()
        parents_b = node_b.parents.copy()

        # Fully remove the merging nodes from the graph
        for child in list(children_a.values()) + list(children_b.values()):
            if new_node.action.name in child.parents:
                del child.parents[new_node.action.name]
        for parent in list(parents_a.values()) + list(parents_b.values()):
            if new_node.action.name in parent.children:
                del parent.children[new_node.action.name]

        # Merge children.
        # Start by adding the non-intersecting children to the dictionary.
        children = ({
            k: children_a.get(k, children_b.get(k)).ResolveToAliveNode()
            for k in (children_a.keys() ^ children_b.keys())
        })
        # For all children that are the same, they must be merged.
        children.update({
            k: MergeNodes(children_a[k].ResolveToAliveNode(),
                          children_b[k].ResolveToAliveNode())
            for k in (children_a.keys() & children_b.keys())
        })

        # Merge parents.
        # Start by adding the non-intersecting parents to the dictionary.
        parents = ({
            k: parents_a.get(k, parents_b.get(k)).ResolveToAliveNode()
            for k in (parents_a.keys() ^ parents_b.keys())
        })
        # For all parents that are the same, they must be merged.
        parents.update({
            k: MergeNodes(parents_a[k].ResolveToAliveNode(),
                          parents_b[k].ResolveToAliveNode())
            for k in (parents_a.keys() & parents_b.keys())
        })

        # Re-add the node back into the graph.
        for child in children.values():
            child = child.ResolveToAliveNode()
            new_node.AddChild(child)
        for parent in parents.values():
            parent = parent.ResolveToAliveNode()
            parent.AddChild(new_node)

        node_a.dead = new_node
        node_b.dead = new_node
        new_node.AssertValidity()
        return new_node

    def MergeChildren(node_a: ActionNode, node_b: ActionNode) -> None:
        assert not node_a.dead
        assert not node_b.dead
        assert node_a.action.name is not node_b.action.name
        node_a.AssertValidity()
        node_b.AssertValidity()

        node_a_name = node_a.action.name
        node_b_name = node_b.action.name

        children_a = list(node_a.children.copy().values())
        children_b = list(node_b.children.copy().values())

        for child in children_a + children_b:
            child = child.ResolveToAliveNode()
            child.AssertValidity()
            child_name = child.action.name
            if (child_name in node_a.children
                    and child_name in node_b.children):
                MergeNodes(node_a.children[child_name],
                           node_b.children[child_name])
            elif child_name in node_a.children:
                if (node_b_name in child.parents):
                    node_b = MergeNodes(node_b, child.parents[node_b_name])
                else:
                    node_b.AddChild(child)
            elif child_name in node_b.children:
                if (node_a_name in child.parents):
                    node_a = MergeNodes(node_a, child.parents[node_a_name])
                else:
                    node_a.AddChild(child)
            else:
                assert False
            node_a = node_a.ResolveToAliveNode()
            node_b = node_b.ResolveToAliveNode()

        resolved_children = ({
            k: node_a.children.get(k).ResolveToAliveNode()
            for k in node_a.children.keys()
        })
        node_a.children = resolved_children.copy()
        node_b.children = resolved_children

        merged_state_check_coverage = MergeStateCheckCoverage(
            node_a.state_check_actions_coverage,
            node_b.state_check_actions_coverage)
        node_a.state_check_actions.update(node_b.state_check_actions)
        node_a.state_check_actions_coverage = merged_state_check_coverage
        node_b.state_check_actions.update(node_a.state_check_actions)
        node_b.state_check_actions_coverage = merged_state_check_coverage.copy(
        )
        node_a.AssertValidity()
        node_b.AssertValidity()

    def AddActionListToGraph(parent: ActionNode, current: ActionNode,
                             action_list: List[Action], end: ActionNode,
                             partial_tests: Set[CoverageTest]) -> None:
        nonlocal root_node
        assert isinstance(parent, ActionNode)
        assert isinstance(end, ActionNode)
        assert len(action_list) > 0
        only_state_check_actions = True
        current.AssertValidity()
        end.AssertValidity()
        for action in action_list:
            assert isinstance(action, Action)
            if action.is_state_check:
                parent.AddStateCheckAction(action, ActionCoverage.PARTIAL)
                parent.partial_coverage_tests.update(partial_tests)
                continue
            only_state_check_actions = False
            if action.name in parent.children:
                current = parent.children[action.name]
            else:
                current = ActionNode(action)
                current.coverage = ActionCoverage.PARTIAL
                parent.AddChild(current)
            current.partial_coverage_tests.update(partial_tests)
            parent = current
        if current == end:
            return
        if only_state_check_actions:
            # If only state_check actions were added, then no need to add a new
            # edge.
            return
        logging.info("Merging children of\n{" + parent.GetGraphPathStr() +
                     "}\n{" + end.GetGraphPathStr() + "}")
        if current.action.name is end.action.name:
            MergeNodes(current, end)
        else:
            MergeChildren(current, end)

        root_node.AssertChildrenValidity()

    def AddPartialPathsHelper(parent: ActionNode, current: ActionNode) -> None:
        nonlocal partial_path_existence
        nonlocal partial_paths
        assert isinstance(current, ActionNode)
        assert isinstance(parent, ActionNode)
        assert current.action.name in parent.children
        # Kick off all of the path replacement before actually doing the
        # replacement, to prevent any unforeseen feedback issues.
        for child in current.children.copy().values():
            AddPartialPathsHelper(current, child)

        if current.action.name not in partial_path_existence:
            return
        for partial_path in partial_paths:
            if current.action.name != partial_path.input_actions[0].name:
                continue
            (end_node,
             test_to_partial_cover) = FollowGraph(parent,
                                                  partial_path.input_actions)
            if end_node is None:
                continue
            if (logging.getLogger().isEnabledFor(logging.INFO)):
                edge_str = " -> ".join(
                    action.name for action in partial_path.output_actions)
                logging.info(f"Inserting edge {{{edge_str}}} into graph at \n"
                             f"{{{parent.GetGraphPathStr()}}}\n"
                             f"{{{end_node.GetGraphPathStr()}}}")
            AddActionListToGraph(parent, current, partial_path.output_actions,
                                 end_node, test_to_partial_cover)

    # Skip the root node, as it is only there for the algorithm to work.
    for child in root_node.children.copy().values():
        AddPartialPathsHelper(root_node, child)


def GenerateGraphvizDotFile(root_node: ActionNode) -> str:
    def GetAllActionNodesAndAssignGraphIds(root: ActionNode
                                           ) -> List[ActionNode]:
        current_graph_id = 0

        def GetAllActionNodesHelper(node):
            nonlocal current_graph_id
            if node.graph_id is not None:
                return []
            node.graph_id = current_graph_id
            current_graph_id += 1
            nodes = [node]
            if not node.children:
                return nodes
            for child in node.children.values():
                nodes.extend(GetAllActionNodesHelper(child))
            return nodes

        # Skip the root node, as it is only there for the algorithm to work.
        all_nodes = []
        for child in root.children.values():
            all_nodes.extend(GetAllActionNodesHelper(child))
        return all_nodes

    def PrintGraph(node: ActionNode) -> List[str]:
        assert node.graph_id is not None, node.action.name
        if not node.children:
            return []
        lines = []
        for child in node.children.values():
            assert child.graph_id is not None, child.action.name
            edge_str = str(node.graph_id) + " -> " + str(child.graph_id)
            lines.append(edge_str)
            lines.extend(PrintGraph(child))
        return lines

    lines = []
    lines.append("strict digraph {")
    nodes = GetAllActionNodesAndAssignGraphIds(root_node)
    for node in nodes:
        color_str = ("seagreen"
                     if node.coverage == ActionCoverage.FULL else "sandybrown")
        label_str = node.GetGraphvizLabel()
        lines.append(f"{node.graph_id}[label={label_str} color={color_str}]")
    # Skip the root node, as it is only there for the algorithm to work.
    for child in root_node.children.values():
        lines.extend(PrintGraph(child))
    lines.append("}")
    return "\n".join(lines)


# Removes any nodes from the graph that are not in the actions dictionary
# given. This completely messes up the the |parents| field, so do not rely
# on that after using this function.
def TrimGraphToAllowedActions(root_node: ActionNode,
                              allowed_actions: Dict[str, Action]) -> None:
    new_children = {}
    for child in root_node.children.values():
        if child.action.name in allowed_actions:
            new_children[child.action.name] = child
    root_node.children = new_children
    new_state_check_actions = {}
    for action in root_node.state_check_actions.values():
        if action.name in allowed_actions:
            new_state_check_actions[action.name] = action
    root_node.state_check_actions = new_state_check_actions
    for child in root_node.children.values():
        TrimGraphToAllowedActions(child, allowed_actions)


def GenerateFrameworkTests(root_node: ActionNode, tests: List[CoverageTest]
                           ) -> List[List[ActionNode]]:
    assert isinstance(root_node, ActionNode)

    def GetAllPaths(node: ActionNode) -> List[List[ActionNode]]:
        assert node is not None
        assert isinstance(node, ActionNode)
        paths = []
        for child in node.children.values():
            for path in GetAllPaths(child):
                assert path is not None
                assert isinstance(path, list)
                assert bool(path)
                paths.append([node] + path)
        if len(paths) == 0:
            paths = [[node]]
        return paths

    def FindLongestFullCoveragePath(paths: List[List[ActionNode]],
                                    test: CoverageTest):
        best = None
        best_length = 0
        for path in paths:
            assert isinstance(path, list)
            len = 0
            for node in path:
                if test in node.full_coverage_tests:
                    len += 100
                elif test in node.partial_coverage_tests:
                    len += 1
            if len > best_length:
                best_length = len
                best = path
        return best

    def PruneNonKeepNodes(node: ActionNode):
        children = {}
        for child in node.children.values():
            if child.keep:
                children[child.action.name] = child
                PruneNonKeepNodes(child)
        node.children = children
        return node

    if len(tests) == 0:
        return []

    all_paths = GetAllPaths(root_node)

    root_node.keep = True
    for test in tests:
        best = FindLongestFullCoveragePath(all_paths, test)
        if best is None:
            continue
        if (logging.getLogger().isEnabledFor(logging.INFO)):
            path_str = ", ".join([node.action.name for node in best])
            logging.info(f"Best path for test {test.name}:\n{path_str}")
        for node in best:
            node.keep = True
    PruneNonKeepNodes(root_node)
    return GetAllPaths(root_node)


def GenerateCoverageFileAndPercents(
        required_coverage_tests: List[CoverageTest],
        tested_graph_root: ActionNode) -> Tuple[str, float, float]:
    lines = []
    total_actions = 0.0
    actions_fully_covered = 0.0
    actions_partially_covered = 0.0
    for coverage_test in required_coverage_tests:
        action_strings = []
        last_action_node = tested_graph_root
        for action in coverage_test.actions:
            total_actions += 1
            coverage = None
            if last_action_node is not None:
                if action.name in last_action_node.children:
                    coverage = last_action_node.children[action.name].coverage
                if (action.is_state_check
                        and last_action_node.HasStateCheckAction(action)):
                    coverage = last_action_node.state_check_actions_coverage[
                        action.name]
            if coverage is None:
                last_action_node = None
                action_strings.append(action.name + '🌑')
                continue
            elif (coverage == ActionCoverage.FULL):
                actions_fully_covered += 1
                action_strings.append(action.name + '🌕')
            elif (coverage == ActionCoverage.PARTIAL):
                action_strings.append(action.name + '🌓')
                actions_partially_covered += 1
            # Only proceed if the action was in the children. If not, then it
            # was in the stateless action list and we stay at the same node.
            if action.name in last_action_node.children:
                last_action_node = last_action_node.children[action.name]
        lines.append(action_strings)

    full_coverage = actions_fully_covered / total_actions
    partial_coverage = ((actions_fully_covered + actions_partially_covered) /
                        total_actions)
    logging.info("Coverage:")
    logging.info(f"Full coverage: {full_coverage:.2f}, "
                 f"Partial coverage: {partial_coverage:.2f}")
    return ("\n".join(["\t".join(line)
                       for line in lines]), full_coverage, partial_coverage)
