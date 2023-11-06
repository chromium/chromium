#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parsing logic to read files for the Web App testing framework.
"""

from collections import defaultdict
import logging
import os
from posixpath import split
import re
from typing import Dict, List, Set, Tuple, Optional

from numpy import append

from models import Action, TestId
from models import ArgEnum
from models import ActionType
from models import ActionsByName
from models import CoverageTest
from models import CoverageTestsByPlatformSet
from models import EnumsByType
from models import PartialAndFullCoverageByBaseName
from models import TestIdsTestNamesByPlatform
from models import TestIdsTestNamesByPlatformSet
from models import TestIdTestNameTuple
from models import TestPartitionDescription
from models import TestPlatform

MIN_COLUMNS_ENUMS_FILE = 2
MIN_COLUMNS_ACTIONS_FILE = 5
MIN_COLUMNS_SUPPORTED_ACTIONS_FILE = 5
MIN_COLUMNS_UNPROCESSED_COVERAGE_FILE = 2


def enumerate_markdown_file_lines_to_table_rows(
        lines: List[str]) -> List[Tuple[int, List[str]]]:
    output = []
    for i, line in enumerate(lines):
        line = line.strip()
        if not line.startswith("|"):
            continue
        # Remove the pipe character from the beginning and end to prevent an
        # empty first and last entry.
        line = line.lstrip('|').rstrip("|")
        row: List[str] = line.split("|")
        if len(row) == 0:
            continue
        stripped = list(map(str.strip, row))
        first_item: str = stripped[0]
        if first_item.startswith("#"):
            continue
        if first_item.count('-') == len(first_item):
            continue
        output.append((i, stripped))
    return output


def enumerate_all_argument_combinations(argument_types: List[ArgEnum]
                                        ) -> List[List[str]]:
    if len(argument_types) == 0:
        return [[]]
    sub_combinations = enumerate_all_argument_combinations(argument_types[:-1])
    last_type = argument_types[-1]
    output: List[List[str]] = []
    for combination in sub_combinations:
        for value in last_type.values:
            output.append(combination + [value])
    return output


def expand_wildcards_in_action(action: str, enums: EnumsByType) -> List[str]:
    """
    Takes an action string that could contain enum wildcards, and returns the
    list of all combinations of actions with all wildcards fully expanded.

    Example input:
    - action: 'Action(EnumType::All, EnumType::All)'
    - enums: {'EnumType': EnumType('EnumType', ['Value1', 'Value2'])}
    Example output:
    - ['Action(Value1, Value1)', 'Action(Value1, Value2)',
       'Action(Value2, Value1)', 'Action(Value2, Value2)']
    """
    if "::All" not in action:
        return [action]
    output: List[str] = []
    for type, enum in enums.items():
        wildcard_str = type + "::All"
        if wildcard_str in action:
            prefix = action[:action.index(wildcard_str)]
            postfix = action[action.index(wildcard_str) + len(wildcard_str):]
            for value in enum.values:
                output.extend(
                    expand_wildcards_in_action(prefix + value + postfix,
                                               enums))
    return output


def expand_tests_from_action_parameter_wildcards(enums: EnumsByType,
                                                 actions: List[str]
                                                 ) -> List[List[str]]:
    """
    Takes a list of actions for a test that could contain argument wildcards.
    Returns a list of tests the expand out all combination of argument
    wildcards.
    Example input:
    - actions: ['Action1(EnumType::All), Action2(EnumType::All)']
    - enums: {'EnumType': EnumType('EnumType', ['Value1', 'Value2'])}
    Example output:
    - [['Action1(Value1)', 'Action2(Value1)'],
       ['Action1(Value1)', 'Action2(Value2)'],
       ['Action1(Value2)', 'Action2(Value1)'],
       ['Action1(Value2)', 'Action2(Value2)']]
    """
    if not actions:
        return [[]]
    current_elements: List[str] = expand_wildcards_in_action(actions[0], enums)
    output: List[List[str]] = []
    following_output = expand_tests_from_action_parameter_wildcards(
        enums, actions[1:])
    for following_list in following_output:
        for element in current_elements:
            output.append([element] + following_list)
    return output


def resolve_bash_style_replacement(output_action_str: str,
                                   argument_values: List[str]):
    for i, arg in enumerate(argument_values):
        find_str = f"${i+1}"
        output_action_str = output_action_str.replace(find_str, arg)
    return output_action_str


def human_friendly_name_to_canonical_action_name(
        human_friendly_action_name: str,
        action_base_name_to_default_args: Dict[str, str]):
    """
    Converts a human-friendly action name (used in the spreadsheet) and turns
    into the format compatible with this testing framework. This does two
    things:
    1) Resolving specified arguments, from "()" format into a "_". For example,
       "action(with_argument)" turns into "action_with_argument".
    2) Resolving modeless actions (that have a default argument) to
       include the default argument. For example, if
       |action_base_name_to_default_arg| contains an entry for
       |human_friendly_action_name|, then that entry is appended to the action.
       "action" and {"action": "default_argument"} will respectively will return
       "action_default_argument".
    If neither of those cases apply, then the |human_friendly_action_name| is
    returned.
    """
    human_friendly_action_name = human_friendly_action_name.strip()
    if human_friendly_action_name in action_base_name_to_default_args:
        # Handle default arguments.
        human_friendly_action_name += "_" + action_base_name_to_default_args[
            human_friendly_action_name]
    elif '(' in human_friendly_action_name:
        # Handle arguments being specified. Also strip trailing _, which appears
        # if the action is "action_name()" without arguments.
        human_friendly_action_name = human_friendly_action_name.replace(
            "(", "_").replace(", ", "_").rstrip(")_")
    return human_friendly_action_name


def read_platform_supported_actions(csv_file
                                    ) -> PartialAndFullCoverageByBaseName:
    """Reads the action base names and coverage from the given csv file.

    Args:
        csv_file: The comma-separated-values file which lists action base names
                  and whether it is fully or partially supported.

    Returns:
        A dictionary of action base name to a set of partially supported
        and fully supported platforms.
    """
    actions_base_name_to_coverage: PartialAndFullCoverageByBaseName = {}
    column_offset_to_platform = {
        0: TestPlatform.MAC,
        1: TestPlatform.WINDOWS,
        2: TestPlatform.LINUX,
        3: TestPlatform.CHROME_OS
    }
    for i, row in enumerate(csv_file):
        if not row:
            continue
        if row[0].startswith("#"):
            continue
        if len(row) < MIN_COLUMNS_SUPPORTED_ACTIONS_FILE:
            raise ValueError(f"Row {i} does not contain enough entries. "
                             f"Got {row}.")
        action_base_name: str = row[0].strip()
        if action_base_name in actions_base_name_to_coverage:
            raise ValueError(f"Action base name '{action_base_name}' on "
                             f"row {i} is already specified.")
        if not re.fullmatch(r'[a-z_]+', action_base_name):
            raise ValueError(
                f"Invald action base name '{action_base_name}' on "
                f"row {i}. Please use snake_case.")
        fully_supported_platforms: Set[TestPlatform] = set()
        partially_supported_platforms: Set[TestPlatform] = set()
        for j, value in enumerate(row[1:5]):
            value = value.strip()
            if not value:
                continue
            if value == "🌕":
                fully_supported_platforms.add(column_offset_to_platform[j])
            elif value == "🌓":
                partially_supported_platforms.add(column_offset_to_platform[j])
            elif value != "🌑":
                raise ValueError(f"Invalid coverage '{value}' on row {i}. "
                                 f"Please use '🌕', '🌓', or '🌑'.")

        actions_base_name_to_coverage[action_base_name] = (
            partially_supported_platforms, fully_supported_platforms)
    return actions_base_name_to_coverage


def read_enums_file(enums_file_lines: List[str]) -> EnumsByType:
    """Reads the enums markdown file.
    """
    enums_by_type: EnumsByType = {}
    for i, row in enumerate_markdown_file_lines_to_table_rows(
            enums_file_lines):
        if len(row) < MIN_COLUMNS_ENUMS_FILE:
            raise ValueError(f"Row {i!r} does not contain enough entries. "
                             f"Got {row}.")
        type = row[0].strip()
        if not re.fullmatch(r'([A-Z]\w*\*?)|', type):
            raise ValueError(f"Invald enum type name {type!r} on row "
                             f"{i!r}. Please use PascalCase.")
        values: List[str] = []
        default_value: Optional[str] = None
        for value in row[1:]:
            value = value.strip()
            if not value:
                continue
            if "*" in value:
                if default_value is not None:
                    raise ValueError(
                        f"Cannot have two default values for enum type "
                        f"{type!r} on row {i!r}.")

                value = value.rstrip("*")
                default_value = value
            if not re.fullmatch(r'([A-Z]\w*\*?)|', value):
                raise ValueError(f"Invald enum value {value!r} on row "
                                 f"{i!r}. Please use PascalCase.")
            values.append(value)
        enum: ArgEnum = ArgEnum(type, values, default_value)
        enums_by_type[enum.type_name] = enum
    return enums_by_type


def read_actions_file(
        actions_file_lines: List[str], enums_by_type: Dict[str, ArgEnum],
        supported_platform_actions: PartialAndFullCoverageByBaseName
) -> Tuple[ActionsByName, Dict[str, str]]:
    """Reads the actions comma-separated-values file.

    If arguments  are specified for an action in the file, then one action is
    added to the results dictionary per action_base_name + mode
    parameterized. A argument marked with a "*" is considered the default
    argument for that action.

    If output actions are specified for an action, then it will be a
    PARAMETERIZED action and the output actions will be resolved into the
    `Action.output_actions` field.

    See the README.md for more information about actions and action templates.

    Args:
        actions_file_lines: The comma-separated-values file read to parse all
                          actions.
        supported_platform_actions: A dictionary of platform to the actions that
                                    are fully or partially covered on that
                                    platform.

    Returns (actions_by_name,
             action_base_name_to_default_args):
        actions_by_name:
            Index of all actions by action name.
        action_base_name_to_default_args:
            Index of action base names to the default arguments. Only populated
            for actions where all argument types have defaults.

    Raises:
        ValueError: The input file is invalid.
    """
    actions_by_name: Dict[str, Action] = {}
    action_base_name_to_default_args: Dict[str, str] = {}
    action_base_names: Set[str] = set()
    for i, row in enumerate_markdown_file_lines_to_table_rows(
            actions_file_lines):
        if len(row) < MIN_COLUMNS_ACTIONS_FILE:
            raise ValueError(f"Row {i!r} does not contain enough entries. "
                             f"Got {row}.")

        shortened_base_name = row[7].strip() if len(row) > 7 else None
        action_base_name = row[0].strip()
        action_base_names.add(action_base_name)
        if not re.fullmatch(r'[a-z_]+', action_base_name):
            raise ValueError(f"Invald action base name {action_base_name} on "
                             f"row {i!r}. Please use snake_case.")

        type = ActionType.STATE_CHANGE
        if action_base_name.startswith("check_"):
            type = ActionType.STATE_CHECK

        output_unresolved_action_names = []
        output_actions_str = row[2].strip()
        if output_actions_str:
            type = ActionType.PARAMETERIZED
            # Output actions for parameterized actions can also specify (or
            # assume default) action arguments (e.g. `do_action(arg1)`) if the
            # parameterized action doesn't have a argument. However, they cannot
            # be fully resolved yet without reading all actions. So the
            # resolution must happen later.
            output_unresolved_action_names = [
                output.strip() for output in output_actions_str.split("&")
            ]

        (partially_supported_platforms,
         fully_supported_platforms) = supported_platform_actions.get(
             action_base_name, (set(), set()))

        # Parse the argument types, and save the defaults if they exist.
        arg_types: List[ArgEnum] = []
        defaults: List[str] = []
        for arg_type_str in row[1].split(","):
            arg_type_str = arg_type_str.strip()
            if not arg_type_str:
                continue
            if arg_type_str not in enums_by_type:
                raise ValueError(
                    f"Cannot find enum type {arg_type_str!r} on row {i!r}.")
            enum = enums_by_type[arg_type_str]
            arg_types.append(enum)
            if enum.default_value:
                defaults.append(enum.default_value)

        # If all arguments types have defaults, then save these defaults as the
        # default argument for this base action name.
        if len(defaults) > 0 and len(defaults) == len(arg_types):
            action_base_name_to_default_args[action_base_name] = (
                "_".join(defaults))

        # From each action row, resolve out the possible parameter arguments
        # and create one action per combination of arguments.

        all_arg_value_combinations: List[List[str]] = (
            enumerate_all_argument_combinations(arg_types))

        for arg_combination in all_arg_value_combinations:
            name = "_".join([action_base_name] + arg_combination)

            # If the action has arguments, then modify the output actions,
            # and cpp method.
            joined_cpp_arguments = ", ".join([
                f"{arg_types[i].type_name}::k{arg}"
                for i, arg in enumerate(arg_combination)
            ])

            # Convert the `cpp_method` to Pascal-case
            cpp_method = ''.join(word.title()
                                 for word in action_base_name.split('_'))
            cpp_method += "(" + joined_cpp_arguments + ")"

            # Resolve bash-replacement for any output actions. Resolving to
            # canonical names is not done here because the defaults map is not
            # fully populated yet.
            output_canonical_action_names: List[str] = []
            for human_friendly_action_name in output_unresolved_action_names:
                bash_replaced_name = resolve_bash_style_replacement(
                    human_friendly_action_name, arg_combination)

                # Handle any wildcards in the actions
                wildcart_expanded_actions = expand_wildcards_in_action(
                    bash_replaced_name, enums_by_type)

                # Output actions for parameterized actions are not allowed to
                # use 'defaults', and the action author must explicitly
                # populate all arguments with bash-style replacements or static
                # values.
                for action_name in wildcart_expanded_actions:
                    output_canonical_action_names.append(
                        human_friendly_name_to_canonical_action_name(
                            action_name, {}))

            if name in actions_by_name:
                raise ValueError(f"Cannot add duplicate action {name} on row "
                                 f"{i!r}")

            action = Action(name, action_base_name, shortened_base_name,
                            cpp_method, type, fully_supported_platforms,
                            partially_supported_platforms)
            action._output_canonical_action_names = (
                output_canonical_action_names)
            actions_by_name[action.name] = action

    unused_supported_actions = set(
        supported_platform_actions.keys()).difference(action_base_names)
    if unused_supported_actions:
        raise ValueError(f"Actions specified as supported that are not in "
                         f"the actions list: {unused_supported_actions}.")

    # Resolve the output actions
    for action in actions_by_name.values():
        if action.type is not ActionType.PARAMETERIZED:
            continue
        assert (action._output_canonical_action_names)
        for canonical_name in action._output_canonical_action_names:
            if canonical_name in actions_by_name:
                action.output_actions.append(actions_by_name[canonical_name])
            else:
                # Having this lookup fail is a feature, it allows a
                # parameterized action to reference output actions that might
                # not all support every value of the parameterized action.
                # When that argument is specified in a test case, then that
                # action would be excluded & one less test case would be
                # generated.
                logging.info(f"Output action {canonical_name} not found for "
                             f"parameterized action {action.name}.")
        if not action.output_actions:
            raise ValueError(
                f"Action {action} is a parameterized action, but "
                f"none of it's possible parameterized actions were"
                f" found: {action._output_canonical_action_names}")
    return (actions_by_name, action_base_name_to_default_args)


def read_unprocessed_coverage_tests_file(
        coverage_file_lines: List[str], actions_by_name: ActionsByName,
        enums_by_type: EnumsByType,
        action_base_name_to_default_arg: Dict[str, str]) -> List[CoverageTest]:
    """Reads the coverage tests markdown file.

    The coverage tests file can have blank entries in the test row, and does not
    have test names.

    Args:
        coverage_file_lines: The comma-separated-values file with all coverage
                           tests.
        actions_by_name: An index of action name to Action
        action_base_name_to_default_arg: An index of action base name to
                                           default argument, if there is one.

    Returns:
        A list of CoverageTests read from the file.

    Raises:
        ValueError: The input file is invalid.
    """
    missing_actions = []
    required_coverage_tests = []
    for i, row in enumerate_markdown_file_lines_to_table_rows(
            coverage_file_lines):
        if len(row) < MIN_COLUMNS_UNPROCESSED_COVERAGE_FILE:
            raise ValueError(f"Row {i!r} does not have test actions: {row}")
        platforms = TestPlatform.get_platforms_from_chars(row[0])
        if len(platforms) == 0:
            raise ValueError(f"Row {i} has invalid platforms: {row[0]}")
        # Filter out all blank actions.
        original_action_strs = [
            action_str for action_str in row[1:] if action_str.strip()
        ]
        # If any of the actions had parameter wildcards (like
        # "WindowOption::All"), then this expands those into multiple tests.
        expanded_tests = expand_tests_from_action_parameter_wildcards(
            enums_by_type, original_action_strs)
        for test_actions in expanded_tests:
            actions: List[Action] = []
            for action_name in test_actions:
                action_name = action_name.strip()
                if action_name == "":
                    continue
                action_name = human_friendly_name_to_canonical_action_name(
                    action_name, action_base_name_to_default_arg)
                if action_name not in actions_by_name:
                    missing_actions.append(action_name)
                    logging.error(f"Could not find action on row {i!r}: "
                                  f"{action_name}")
                    continue
                actions.append(actions_by_name[action_name])
            coverage_test = CoverageTest(actions, platforms)
            required_coverage_tests.append(coverage_test)
    if missing_actions:
        raise ValueError(f"Actions missing from actions dictionary: "
                         f"{', '.join(missing_actions)}")
    return required_coverage_tests


def get_and_maybe_delete_tests_in_browsertest(
    filename: str,
    required_tests: Set[TestIdTestNameTuple] = {},
    delete_in_place: bool = False
) -> Dict[TestIdTestNameTuple, Set[TestPlatform]]:
    """
    Returns a dictionary of all test ids and test names found to
    the set of detected platforms the test is enabled on.

    When delete_in_place is set to True, overwrite the file to remove tests not
    in required_tests.

    For reference, this is what a disabled test by a sheriff typically looks
    like:

    TEST_F(WebAppIntegrationTestBase, DISABLED_NavStandalone_InstallIconShown) {
        ...
    }

    In the above case, the test will be considered disabled on all platforms.
    This is what a test disabled by a sheriff on a specific platform looks like:

    #if BUILDFLAG(IS_WIN)
    #define MAYBE_NavStandalone_InstallIconShown \
            DISABLED_NavStandalone_InstallIconShown
    #else
    #define MAYBE_NavStandalone_InstallIconShown NavStandalone_InstallIconShown
    #endif
    TEST_F(WebAppIntegrationTestBase, MAYBE_NavStandalone_InstallIconShown) {
        ...
    }

    In the above case, the test will be considered disabled on
    `TestPlatform.WINDOWS` and thus enabled on {`TestPlatform.MAC`,
    `TestPlatform.CHROME_OS`, and `TestPlatform.LINUX`}.
    """
    tests: Dict[TestIdTestNameTuple, Set[TestPlatform]] = {}

    with open(filename, 'r') as fp:
        file = fp.read()
        result_file = file
        # Attempts to match a full test case, where the name contains the test
        # id prefix. Purposefully allows any prefixes on the test name (like
        # MAYBE_ or DISABLED_). Examples can be found here.
        # https://regex101.com/r/l1xnAJ/2
        for match in re.finditer(
                'IN_PROC_BROWSER_TEST_F[\\(\\w\\s,]+'
                fr'{CoverageTest.TEST_ID_PREFIX}([a-zA-Z0-9._-]+)\)'
                '\\s*{\n(?:\\s*\\/\\/.*\n)+((?:[^;^}}]+;\n)+)}', file):
            test_steps: List[str] = []
            if match.group(2):
                test_body = match.group(2).split(";")
                for line in test_body:
                    assert not line.strip().startswith("//")
                    test_steps.append(line.strip())
            test_id = generate_test_id_from_test_steps(test_steps)
            test_name = match.group(1)
            tests[TestIdTestNameTuple(test_id, test_name)] = set(TestPlatform)
            browser_test_name = f"{CoverageTest.TEST_ID_PREFIX}{test_name}"
            required_tests_ids = []
            for t in required_tests:
                required_tests_ids.append(t[0])
            if f"DISABLED_{browser_test_name}" not in file:
                if delete_in_place and test_id not in required_tests_ids:
                    del tests[TestIdTestNameTuple(test_id, test_name)]
                    # Remove the matching test code block when the test is not
                    # in required_tests
                    regex_to_remove = re.escape(match.group(0))
                    result_file = re.sub(regex_to_remove, '', result_file)
                continue
            enabled_platforms: Set[TestPlatform] = tests[TestIdTestNameTuple(
                test_id, test_name)]
            for platform in TestPlatform:
                # Search for macro that specifies the given platform before
                # the string "DISABLED_<test_name>".
                macro_for_regex = re.escape(platform.macro)
                # This pattern ensures that there aren't any '{' or '}'
                # characters between the macro and the disabled test name, which
                #  ensures that the macro is applying to the correct test.
                if re.search(
                        fr"{macro_for_regex}[^{{}}]+DISABLED_{browser_test_name}",
                        file):
                    enabled_platforms.remove(platform)
            if len(enabled_platforms) == len(TestPlatform):
                enabled_platforms.clear()
    if delete_in_place:
        with open(filename, 'w') as fp:
            fp.write(result_file)
    return tests


def find_existing_and_disabled_tests(
    test_partitions: List[TestPartitionDescription],
    required_coverage_by_platform_set: CoverageTestsByPlatformSet,
    delete_in_place: bool = False
) -> Tuple[TestIdsTestNamesByPlatformSet, TestIdsTestNamesByPlatform]:
    """
    Returns a dictionary of platform set to test id, and a dictionary of
    platform to disabled test ids.
    """
    existing_tests: TestIdsNamesByPlatformSet = defaultdict(lambda: set())
    disabled_tests: TestIdsNamesByPlatform = defaultdict(lambda: set())
    for partition in test_partitions:
        for file in os.listdir(partition.browsertest_dir):
            if not file.startswith(partition.test_file_prefix):
                continue
            platforms = frozenset(
                TestPlatform.get_platforms_from_browsertest_filename(file))
            filename = os.path.join(partition.browsertest_dir, file)
            required_tests = set(
                TestIdTestNameTuple(i.id, i.generate_test_name())
                for i in required_coverage_by_platform_set.get(platforms, []))
            tests = get_and_maybe_delete_tests_in_browsertest(
                filename, required_tests, delete_in_place)
            for test_id, test_name in tests.keys():
                if test_id in existing_tests[platforms]:
                    raise ValueError(f"Already found test {test_name}. "
                                     f"Duplicate test in {filename}")
                existing_tests[platforms].add(
                    TestIdTestNameTuple(test_id, test_name))
            for platform in platforms:
                for (test_id, test_name), enabled_platforms in tests.items():
                    if platform not in enabled_platforms:
                        disabled_tests[platform].add(
                            TestIdTestNameTuple(test_id, test_name))
            test_names = [test_name for (test_id, test_name) in tests.keys()]
            logging.info(f"Found tests in {filename}:\n{test_names}")
    return (existing_tests, disabled_tests)


def generate_test_id_from_test_steps(test_steps: List[str]) -> str:
    test_id = []
    for test_step in test_steps:
        # Examples of the matching regex.
        # https://regex101.com/r/UYlzkK/1
        match_test_step = re.search(r"helper_.(\w+)\(([\w,\s:]*)\)", test_step)
        if match_test_step:
            actions = re.findall('[A-Z][^A-Z]*', match_test_step.group(1))
            test_id += [a.lower() for a in actions]
            if match_test_step.group(2):
                parameters = [
                    m.strip() for m in match_test_step.group(2).split(',')
                ]
                for p in parameters:
                    match_param_value = re.match(r".*::k(.*)", p)
                    if match_param_value.group(1):
                        test_id.append(match_param_value.group(1))
    return "_".join(test_id)
