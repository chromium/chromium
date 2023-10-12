#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Classes used to process the Web App testing framework data.
"""

import collections
from enum import Enum
from enum import unique
import os
from typing import FrozenSet, Optional, Set, List, Dict, Tuple

TestId = str


@unique
class ActionCoverage(Enum):
    PARTIAL = 1
    FULL = 2
    NONE = 3


@unique
class ActionType(Enum):
    # The action changes the state of the system.
    STATE_CHANGE = 1
    # The action checks the state of the system (and does not change it).
    STATE_CHECK = 2
    # The action generates one or more other actions, allowing a parameterized
    # action to produce multiple tests.
    PARAMETERIZED = 3


@unique
class TestPlatform(Enum):
    """
    Describes a platform that is being tested.
    Attributes:
        macro: This is the macro used in a browsertest file by a gardener to
               specify the platform. Formatted for use in regex.
        char: This is the character used in the unprocessed required coverage
              test spreadsheet to specify if a test applies to a given platform.
        suffix: The suffix applied to browsertest files to specify that the
                file runs on that platform.
    """
    MAC = ("BUILDFLAG(IS_MAC)", "M", "mac")
    WINDOWS = ("BUILDFLAG(IS_WIN)", "W", "win")
    LINUX = ("BUILDFLAG(IS_LINUX)", "L", "linux")
    CHROME_OS = ("BUILDFLAG(IS_CHROMEOS)", "C", "cros")

    def __init__(self, macro: str, char: str, suffix: str):
        self.macro: str = macro
        self.char: str = char
        self.suffix: str = suffix

    @staticmethod
    def get_platforms_from_chars(string: str) -> Set["TestPlatform"]:
        """Parse the string for platforms.
        M = Mac, L = Linux, W = Windows, and C = ChromeOS
        Example inputs: "MLWC", "WLM", "C", ""
        """
        result = set()
        for platform in TestPlatform:
            if platform.char in string:
                result.add(platform)
        return result

    @staticmethod
    def get_test_fixture_suffix(platforms: Set["TestPlatform"]) -> str:
        """Output a string from the platforms to suffix the test fixture name.
        If all platforms are specified, the suffix is blank.
        Example: {MAC, LINUX} outputs "MacLinux"
        """
        if len(platforms) == len(TestPlatform):
            return ""
        result: str = ""
        for platform in TestPlatform:
            if platform in platforms:
                result += platform.suffix.capitalize()
        return result

    @staticmethod
    def get_platforms_from_browsertest_filename(filename: str
                                                ) -> Set["TestPlatform"]:
        result = set()
        for platform in TestPlatform:
            if platform.suffix in filename:
                result.add(platform)
        if not result:
            result.update(TestPlatform)
        return result


class ArgEnum:
    """Represents an enumeration used as an argument in an action."""

    def __init__(self, type_name: str, values: List[str],
                 default_value: Optional[str]):
        assert type_name is not None
        assert values is not None
        assert len(values) != 0
        self.type_name = type_name
        self.values = values
        self.default_value = default_value


class Action:
    """Represents a user action.

    Attributes:
        name: The name of the action often can contain arguments, which are
              resolved at parse time.
        base_name: If the action has arguments, this is the base action name
                   before the argument was concatenated, resulting in the name.
        cpp_method: Resolved method to call in C++ to execute this action. This
                    includes any arguments.
        type: The type of the action (see ActionType).
        output_actions: Only populated if `type` is PARAMETERIZED. These are the
                        possible actions that can take the place of this action
                        when generating processed tests.
        full_coverage_platforms: Platforms where this action is fully supported.
        partial_coverage_platforms: Platforms where this action is partially
                                    supported.
    """

    def __init__(self, name: str, base_name: str, shortened_base_name: str,
                 cpp_method: str, type: ActionType,
                 full_coverage_platforms: Set[TestPlatform],
                 partial_coverage_platforms: Set[TestPlatform]):
        assert name is not None
        assert base_name is not None
        assert type is not None
        self.name: str = name
        self.base_name: str = base_name
        self.shortened_base_name: str = shortened_base_name
        self.cpp_method: str = cpp_method
        self.type: ActionType = type
        self.output_actions: List[Action] = []
        self.full_coverage_platforms: Set[
            TestPlatform] = full_coverage_platforms
        self.partial_coverage_platforms: Set[
            TestPlatform] = partial_coverage_platforms
        # Used in `read_action_files` as temporary storage.
        self._output_canonical_action_names: List[str] = []

    def get_coverage_for_platform(self,
                                  platform: TestPlatform) -> ActionCoverage:
        if platform in self.full_coverage_platforms:
            return ActionCoverage.FULL
        if platform in self.partial_coverage_platforms:
            return ActionCoverage.PARTIAL
        return ActionCoverage.NONE

    def supported_for_platform(self, platform: TestPlatform) -> bool:
        return (platform in self.full_coverage_platforms
                or platform in self.partial_coverage_platforms)

    def is_state_check(self) -> bool:
        return self.type == ActionType.STATE_CHECK

    def __str__(self):
        return (f"Action[{self.name}, "
                f"base_name: {self.base_name}, "
                f"shortened_base_name: {self.shortened_base_name}, "
                f"type: {self.type}, "
                f"output_actions: "
                f"{[a.name for a in self.output_actions]}, "
                f"full_coverage_platforms: "
                f"{[p.char for p in self.full_coverage_platforms]}, "
                f"partial_coverage_platforms: "
                f"{[p.char for p in self.partial_coverage_platforms]}]")


class ActionNode:
    """Represents a node in an action graph, where all nodes are
    state change actions.

    Attributes:
        action: The action this node represents.
        children: The children of this node, keyed by action name.
        state_check_actions: State checks that can be performed on this node.
        graph_id: Used for graphviz file generation.
    """

    @staticmethod
    def CreateRootNode():
        return ActionNode(
            Action("root", "root", "root", "root()", ActionType.STATE_CHANGE,
                   set(), set()))

    def __init__(self, action: Action):
        assert action is not None
        assert action.type == ActionType.STATE_CHANGE
        self.action: Action = action
        self.children: Dict[str, "ActionNode"] = {}
        self.state_check_actions: Dict[str, Action] = {}
        self.graph_id: Optional[int] = None

    def add_child(self, child: 'ActionNode'):
        assert child is not self
        assert not child.action.name in self.children
        self.children[child.action.name] = child

    def add_state_check_action(self, action: Action):
        assert action is not None and isinstance(action, Action)
        assert action.is_state_check(), action
        self.state_check_actions[action.name] = action

    def has_state_check_action(self, action: Action) -> bool:
        assert action.is_state_check()
        return action.name in self.state_check_actions

    def get_graphviz_label(self) -> str:
        node_str = "< <B>" + self.action.name + "</B>"
        if self.state_check_actions:
            node_str += "<BR/>(" + ", ".join(
                [action_name
                 for action_name in self.state_check_actions]) + ")"
        return node_str + " >"

    def __str__(self):
        return (f"ActionNode[{self.action.name}, "
                f"children: {self.children.keys()}")


class CoverageTest:
    """Represents a test with a list of actions

    Attributes:
        id: Unique name, or identifier, of the test.
        actions: list of actions the test specifies to execute.
        platforms: set of platforms this test is run on.
    """

    TEST_ID_PREFIX = "WAI_"

    def __init__(self, actions: List[Action], platforms: Set[TestPlatform]):
        assert actions is not None
        assert platforms is not None
        self.id: TestId = "_".join([a.name for a in actions])
        self.actions: List[Action] = actions
        self.platforms: Set[TestPlatform] = platforms

    def generate_browsertest(self, test_partition: "TestPartitionDescription"
                             ) -> str:
        comments = [
            "Test contents are generated by script. Please do not modify!",
            "See `docs/webapps/why-is-this-test-failing.md` or",
            "`docs/webapps/integration-testing-framework` for more info.",
            "Gardeners: Disabling this test is supported."
        ]
        body = ''.join(["  // " + comment + "\n" for comment in comments])
        body += '\n'.join([(f"  helper_.{action.cpp_method};")
                           for action in self.actions])
        fixture = f"{test_partition.test_fixture}"
        return (f"IN_PROC_BROWSER_TEST_F("
                f"{fixture}, "
                f"{CoverageTest.TEST_ID_PREFIX}{self.generate_test_name()}) "
                f"{{\n{body}\n}}")

    def generate_test_name(self):
        state_change_list = []
        for a in self.actions:
            if "check" not in a.name:
                action_name = (a.shortened_base_name
                               if a.shortened_base_name else a.base_name)
                action = a.name.replace(a.base_name, action_name)
                action_list = action.split("_")
                state_change = "".join(a[0].upper() + a[1:]
                                       for a in action_list)
                state_change_list.append(state_change)
        return "_".join(state_change_list)

    def __str__(self):
        return (f"CoverageTest[id: {self.id}, "
                f"actions: {[a.name for a in self.actions]}, "
                f"platforms: {[str(p) for p in self.platforms]}]")


class TestPartitionDescription:
    """
    Due to framework implementation limitations, some tests need to be split
    into different browsertest files. This class describes that partition.

    Attributes:
        action_name_prefixes: If a test has any actions that have any of these
                              prefixes, then that tests is considered part of
                              this partition.
        browsertest_dir: The directory where the browsertests for this partition
                         live.
        test_file_prefix: The filename prefix used for the expected browsertest
                          file. The full filename is expected to optionally
                          contain per-platform suffixes, followed by a ".cc".
        test_fixture: The gtest fixture used when printing the test declaration.
    """

    def __init__(self, action_name_prefixes: Set[str], browsertest_dir: str,
                 test_file_prefix: str, test_fixture: str):
        self.action_name_prefixes: Set[str] = action_name_prefixes
        self.browsertest_dir: str = browsertest_dir
        self.test_file_prefix: str = test_file_prefix
        self.test_fixture: str = test_fixture

    def generate_browsertest_filepath(self, platforms: Set[TestPlatform]):
        """
        Genenerates the browsertest file path given the set of platforms. The
        suffixes are deterministically ordered by the TestPlatform enum order.
        Example:
            self.browsertest_dir = "/path/to/test"
            self.test_file_prefix = "tests_file"
            platforms = {TestPlatform.WINDOWS, TestPlatform.MAC}

            return "/path/to/test/tests_file_mac_win.cc"
        """
        suffix = ""
        if len(platforms) != len(TestPlatform):
            # Iterating TestPlatform to get stable ordering.
            for platform in TestPlatform:
                if platform in platforms:
                    suffix += "_" + platform.suffix
        return (os.path.join(self.browsertest_dir,
                             (self.test_file_prefix + suffix + ".cc")))


TestIdTestNameTuple = collections.namedtuple("TestIdTestNameTuple",
                                             "test_id, test_name")
TestIdsTestNamesByPlatform = Dict[TestPlatform, Set[TestIdTestNameTuple]]
TestIdsTestNamesByPlatformSet = Dict[FrozenSet[TestPlatform],
                                     Set[TestIdTestNameTuple]]
CoverageTestsByPlatformSet = Dict[FrozenSet[TestPlatform], List[CoverageTest]]
CoverageTestsByPlatform = Dict[TestPlatform, List[CoverageTest]]
EnumsByType = Dict[str, ArgEnum]
ActionsByName = Dict[str, Action]
EnumsByType = Dict[str, ArgEnum]
PartialAndFullCoverageByBaseName = Dict[
    str, Tuple[Set[TestPlatform], Set[TestPlatform]]]
