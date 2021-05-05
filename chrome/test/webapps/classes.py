#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Classes used to process the Web App testing framework data.
"""

from enum import Enum
from enum import unique


@unique
class ActionCoverage(Enum):
    PARTIAL = 1
    FULL = 2
    NONE = 3


class Action:
    """Represents a user action.

    Attributes:
        name: The name of the action often can contain a parameter, which is
              resolved at parse time.
        base_name: If the action has parameters, this is the base action name
                   before the parameter was concatenated, resulting in the name.
        is_state_check: If the action only inspects state, and does not change
                       state.
    """

    def __init__(self, name: str, base_name: str, is_state_check: bool):
        assert name is not None
        assert base_name is not None
        assert is_state_check is not None
        self.name = name
        self.base_name = base_name
        self.is_state_check = is_state_check

    def __str__(self):
        return (f"Action[{self.name!r}, "
                f"base_name: {self.base_name!r}, "
                f"state_check: {self.is_state_check!r}]")


class ActionNode:
    """Represents a node in an action graph, where all nodes are
    non-state_check actions.

    Attributes:
        action: The action this node represents.
        coverage: The test coverage of this node.
        children: The children of this node, keyed by action name.
        parents: The parents of this node, keyed by action name.
        full_coverage_tests: The tests that this node helps fully cover.
        partial_coverage_tests: The tests that this node helps partially cover.
        state_check_actions: State checks that can be performed on this node.
        state_check_actions_coverage: Coverage of the given state_check action.
        graph_id: Used for graphviz file generation.
        dead: Used for debugging / assertions. Records the replacement node if
              this one is dead.
        keep: Used for graph pruning to signify this node should be kept.
    """

    def __init__(self, action: Action):
        assert action is not None
        assert not action.is_state_check
        self.action = action
        self.coverage = None
        self.children = {}
        self.parents = {}
        self.full_coverage_tests = set()
        self.partial_coverage_tests = set()
        self.state_check_actions = {}
        self.state_check_actions_coverage = {}
        self.graph_id = None
        self.dead = None
        self.keep = False

    def AssertChildrenValidity(self):
        assert not self.dead, self.GetGraphPathStr()
        for child in self.children.values():
            assert self.action.name in child.parents, (self.GetGraphPathStr() +
                                                       "\nChild:\n" +
                                                       child.GetGraphPathStr())
            assert child.parents[self.action.name] == self, (
                "Me:\n" + self.GetGraphPathStr() + "\nChild:\n" +
                child.GetGraphPathStr() + "\nChild thinks I am:\n" +
                child.parents[self.action.name].GetGraphPathStr())
            assert child is not self
            for parent in child.parents.values():
                assert not parent.dead
                assert not parent.children[child.action.name].dead
                assert parent.children[child.action.name] == child
            child.AssertChildrenValidity()

    def AssertParentValidity(self):
        assert not self.dead, self.GetGraphPathStr()
        for parent in self.parents.values():
            assert parent.children[self.action.name] == self, (
                "Me:\n" + self.GetGraphPathStr() + "\nParent:\n" +
                parent.GetGraphPathStr() + "\nParent thinks I am:\n" +
                parent.children[self.action.name].GetGraphPathStr())
            assert parent.children[self.action.name] == self
            assert parent is not self, parent.action.name
            for child in parent.children.values():
                assert child.parents[parent.action.name] == parent
                assert not child.parents[parent.action.name].dead
            parent.AssertParentValidity()

    def AssertValidity(self):
        self.AssertChildrenValidity()
        self.AssertParentValidity()

    def HasChild(self, child: 'ActionNode') -> bool:
        return (child.action.name in self.children
                and self.children[child.action.name] is child
                and self.action.name in child.parents
                and child.parents[self.action.name] is self)

    def AddChild(self, child: 'ActionNode'):
        assert child is not self
        assert not self.dead
        assert not child.dead
        assert not self.HasChild(child), (child.action.name + " already in " +
                                          self.action.name)
        assert self.action.name not in child.parents, (self.action.name +
                                                       " already parent of " +
                                                       child.action.name)
        self.children[child.action.name] = child
        child.parents[self.action.name] = self

    def RemoveChild(self, child: 'ActionNode'):
        assert child is not self
        assert not self.dead
        assert not child.dead
        assert child.action.name in self.children, (child.action.name +
                                                    " not in " +
                                                    self.action.name)
        assert self.action.name in child.parents, (self.action.name +
                                                   " not parent of " +
                                                   child.action.name)
        del self.children[child.action.name]
        del child.parents[self.action.name]

    def AddStateCheckAction(self, action: Action, coverage: ActionCoverage):
        assert action is not None and isinstance(action, Action)
        assert action.is_state_check
        assert coverage is not None and isinstance(coverage, ActionCoverage)
        self.state_check_actions[action.name] = action
        if (action.name not in self.state_check_actions_coverage
                or coverage is ActionCoverage.FULL):
            self.state_check_actions_coverage[action.name] = coverage

    def HasStateCheckAction(self, action: Action) -> bool:
        assert action.is_state_check
        return action.name in self.state_check_actions

    def ResolveToAliveNode(self) -> 'ActionNode':
        if not self.dead:
            return self
        return self.dead.ResolveToAliveNode()

    def GetGraphvizLabel(self) -> str:
        node_str = "< <B>" + self.action.name + "</B>"
        if self.state_check_actions:
            node_str += "<BR/>(" + ", ".join(
                [action_name
                 for action_name in self.state_check_actions]) + ")"
        return node_str + " >"

    def GetGraphPathStr(self) -> str:
        if len(self.parents) == 0:
            return self.action.name
        if len(self.parents) == 1:
            return (list(self.parents.values())[0].GetGraphPathStr() + " -> " +
                    self.action.name)
        parent_strs = []
        for parent in self.parents.values():
            parent_strs.append(parent.GetGraphPathStr())
        return ("Branched: [" + " | ".join(parent_strs) + "] -> " +
                self.action.name)

    def __str__(self):
        return (
            f"ActionNode[{self.action.name!r}, "
            f"coverage: {self.coverage!r}, "
            f"children: {', '.join(self.children.keys())!r}, "
            f"parents: {', '.join(self.parents.keys())!r}, "
            f"full_coverage_tests: "
            f"{', '.join([t.name for t in self.full_coverage_tests])!r}, "
            f"partial_coverage_tests: "
            f"{', '.join([t.name for t in self.partial_coverage_tests])!r}]")


class PartialCoverageAddition:
    """Represents a partial coverage path addition, where, if the input actions
    are found, then the output actions are added as a new edge to the graph.

    Attributes:
        input_actions: Action sequence to look for in a directed action graph.
        output_actions: Actions to augment the graph with if the input actions
                        are found.
    """

    def __init__(self):
        self.input_actions = []
        self.output_actions = []

    def Reverse(self):
        temp = self.input_actions
        self.input_actions = self.output_actions
        self.output_actions = temp

    def __str__(self):
        return (f"PartialCoverageAddition[input_actions: "
                f"{', '.join([a.name for a in self.input_actions])!r}, "
                f"output_actions: "
                f"{', '.join([a.name for a in self.output_actions])!r}]")


class CoverageTest:
    """Represents a test with a list of actions

    Attributes:
        name: Unique name, or identifier, of the test.
        actions: list of actions the test specifies to execute.
    """

    def __init__(self, name):
        assert name is not None
        assert isinstance(name, str)
        self.name = name
        self.actions = []

    def __str__(self):
        return (f"CoverageTest[name: {self.name!r}, actions: "
                f"{', '.join([a.name for a in self.actions])}]")
