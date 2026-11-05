# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for components/feature_engagement.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

_FEATURE_LIST_H_PATH = (
    'components/feature_engagement/public/feature_list.h'
)
_FEATURE_LIST_CC_PATH = (
    'components/feature_engagement/public/feature_list.cc'
)
_FEATURE_CONSTANTS_H_PATH = (
    'components/feature_engagement/public/feature_constants.h'
)
_FEATURE_CONSTANTS_CC_PATH = (
    'components/feature_engagement/public/feature_constants.cc'
)
_FEATURE_CONSTANTS_PATH = (
    'components/feature_engagement/public/android/java/src/org/chromium/components/feature_engagement/FeatureConstants.java'
)
_EVENT_CONSTANTS_PATH = (
    'components/feature_engagement/public/android/java/src/org/chromium/components/feature_engagement/EventConstants.java'
)
_ENTERPRISE_POLICY_ALLOWLIST_PATH = (
    'components/feature_engagement/internal/android/java/src/org/chromium/components/feature_engagement/internal/EnterprisePolicyBypassAllowlist.java'
)
_TRACKER_IMPL_JAVA_PATH = (
    'components/feature_engagement/internal/android/java/src/org/chromium/components/feature_engagement/internal/TrackerImpl.java'
)
_UI_FORMAT_JAVA_PATH = (
    'components/feature_engagement/public/android/java/src/org/chromium/components/feature_engagement/UiFormat.java'
)
_FEATURE_PROPERTIES_JAVA_PATH = (
    'components/feature_engagement/public/android/java/src/org/chromium/components/feature_engagement/FeatureProperties.java'
)
_ACTIONS_XML_PATH = (
    'tools/metrics/actions/actions.xml'
)
_HISTOGRAMS_XML_PATH = (
    'tools/metrics/histograms/metadata/feature_engagement/histograms.xml'
)

_BYPASS_POLICY_TAG = (
    '[UserEducation.BypassPolicyCheck]'
)
_BYPASS_POLICY_TAG_RE = (
    r'^\s*\[UserEducation\.BypassPolicyCheck\]\s*=\s*\S.*$'
)
_NEW_UI_FORMAT_TAG = (
    '[UserEducation.NewUiFormat]'
)
_NEW_UI_FORMAT_TAG_RE = (
    r'^\s*\[UserEducation\.NewUiFormat\]\s*=\s*\S.*$'
)

_USER_EDUCATION_JAVA_POLICY_OWNERS = (
    'jhimawan@google.com',
    'mschillaci@google.com',
)

_ALLOWED_JNI_FILES = (
    _TRACKER_IMPL_JAVA_PATH,
    'components/feature_engagement/internal/android/tracker_impl_android.cc',
    'components/feature_engagement/internal/android/tracker_impl_android.h',
    'components/feature_engagement/public/android/java/src/org/chromium/components/feature_engagement/CppWrappedTestTracker.java',
    'components/feature_engagement/public/android/wrapping_test_tracker.cc',
    'components/feature_engagement/public/android/wrapping_test_tracker.h',
    'chrome/browser/feature_engagement/java/src/org/chromium/chrome/browser/feature_engagement/TrackerFactory.java',
    'chrome/browser/feature_engagement/tracker_factory.cc',
)


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def _CommonChecks(input_api, output_api):
    results = []
    results.extend(_CheckJavaConstantsSorting(input_api, output_api))
    results.extend(_CheckFeatureListSorting(input_api, output_api))
    results.extend(_CheckNoComparatorAny(input_api, output_api))
    results.extend(_CheckNoRedundantNamespaceQualifier(input_api, output_api))
    results.extend(_CheckAlwaysTrueInUsedOrTrigger(input_api, output_api))
    results.extend(_CheckEventConfigComparatorAndWindow(input_api, output_api))
    results.extend(_CheckNewFlagsHaveMetrics(input_api, output_api))
    results.extend(_CheckEnterprisePolicyBypass(input_api, output_api))
    results.extend(_CheckUiFormatAdditions(input_api, output_api))
    results.extend(_CheckNoUnauthorizedTrackerJni(input_api, output_api))
    return results


def _CheckFeatureListSorting(input_api, output_api):
    results = []
    for f in input_api.AffectedFiles():
        if f.LocalPath() not in (_FEATURE_LIST_H_PATH, _FEATURE_LIST_CC_PATH):
            continue

        for line_num, line in f.ChangedContents():
            if '#if BUILDFLAG' in line:
                message = (
                    f'It looks like you are adding a new BUILDFLAG block to '
                    f'{f.LocalPath()}. Please re-use the existing blocks and keep '
                    f'the items in alphabetical order within those blocks. If there '
                    f'is no existing block that matches your new block, then you can '
                    f'ignore this message. For questions or if this is too noisy, '
                    f'please ping mschillaci@.'
                )
                results.append(output_api.PresubmitPromptWarning(message))
                break

    return results


def _CheckJavaConstantsSorting(input_api, output_api):
    results = []
    for f in input_api.AffectedFiles():
        local_path = f.UnixLocalPath()
        if local_path == _FEATURE_CONSTANTS_PATH:
            results.extend(_CheckFeatureConstantsFile(input_api, output_api, f))
        elif local_path == _EVENT_CONSTANTS_PATH:
            results.extend(_CheckEventConstantsFile(input_api, output_api, f))
        elif local_path == _ENTERPRISE_POLICY_ALLOWLIST_PATH:
            results.extend(
                _CheckEnterprisePolicyAllowlistFile(input_api, output_api, f)
            )

    return results


def _CheckFeatureConstantsFile(input_api, output_api, affected_file):
    new_contents = affected_file.NewContents()
    file_path = affected_file.LocalPath()
    results = []

    # 1. Check @StringDef block
    results.extend(
        _CheckKeepSortedBlock(input_api, output_api, new_contents, file_path)
    )

    # 2. Check String constants block
    results.extend(
        _CheckStringConstantsSorted(
            input_api,
            output_api,
            new_contents,
            file_path,
            r'^\s*String\s+([A-Z0-9_]+)\s*=',
        )
    )

    return results


def _CheckEventConstantsFile(input_api, output_api, affected_file):
    new_contents = affected_file.NewContents()
    file_path = affected_file.LocalPath()
    results = []

    # Check String constants block
    # Rules: if a line startsWith 'public static final String ',
    # then it gets added to our list
    results.extend(
        _CheckStringConstantsSorted(
            input_api,
            output_api,
            new_contents,
            file_path,
            r'^\s*public\s+static\s+final\s+String\s+([A-Z0-9_]+)\s*=',
        )
    )

    return results


def _CheckEnterprisePolicyAllowlistFile(input_api, output_api, affected_file):
    new_contents = affected_file.NewContents()
    file_path = affected_file.LocalPath()
    return _CheckKeepSortedBlock(input_api, output_api, new_contents, file_path)


def _CheckKeepSortedBlock(input_api, output_api, lines, file_path):
    in_block = False  # noqa: F841
    block_lines = []
    for line in lines:
        stripped = line.strip()
        if stripped and stripped.startswith('FeatureConstants.'):
            block_lines.append(stripped)

    if not block_lines:
        return []

    sorted_block = sorted(block_lines)
    if block_lines == sorted_block:
        return []

    # Find discrepancy
    for i in range(len(block_lines)):
        if block_lines[i] != sorted_block[i]:
            message = (
                f'The @StringDef block in {file_path} is not sorted alphabetically.\n'
                f'  - Actual item:   {block_lines[i]}\n'
                f'  - Expected item: {sorted_block[i]}'
            )
            return [output_api.PresubmitPromptWarning(message)]
    return []


def _CheckStringConstantsSorted(
    input_api, output_api, lines, file_path, regex_pattern
):
    string_constants = []
    for line in lines:
        # Match constant declaration based on regex_pattern
        # Note: Using input_api.re for regex as is standard in Chromium presubmits.
        match = input_api.re.search(regex_pattern, line)
        if match:
            string_constants.append(match.group(1))

    if not string_constants:
        return []

    sorted_constants = sorted(string_constants)
    if string_constants == sorted_constants:
        return []

    # Find discrepancy
    for i in range(len(string_constants)):
        if string_constants[i] != sorted_constants[i]:
            message = (
                f'The String constants in {file_path} are not sorted alphabetically.'
                f'\n  - Actual item:   {string_constants[i]}\n'
                f'  - Expected item: {sorted_constants[i]}'
            )
            return [output_api.PresubmitError(message)]
    return []


_COMPARATOR_ANY_CALL_PATTERN = r'\bComparator\s*\(\s*ANY\b'

_ANY_COMPARATOR_PATTERN = (
    r'\b(?:'
    r'kAlwaysTrue|kAlwaysAvailable|kNoRestrictions?'
    r'|Comparator\s*\(\s*(?:ANY\b[^)]*)?\)'
    r')'
)


def _IterAffectedCppFiles(input_api):
    for f in input_api.AffectedFiles():
        local_path = f.LocalPath()
        if not local_path.endswith(('.cc', '.h', '.mm', '.cpp')):
            continue

        # Skip unit tests that test Comparator/validator edge cases.
        if local_path.endswith(('_unittest.cc', '_unittest.h')):
            continue

        # Skip configuration.h where the constants are defined.
        if local_path.endswith(
            'components/feature_engagement/public/configuration.h'
        ):
            continue

        yield f


def _IterChangedStatements(input_api, affected_file):
    stmt_line = None
    stmt_parts = []
    for line_num, line in affected_file.ChangedContents():
        code = line.split('//', 1)[0]
        code = input_api.re.sub(r'/\*.*?\*/', '', code).strip()
        if not code:
            continue
        if stmt_line is None:
            stmt_line = line_num
        stmt_parts.append(code)
        joined = ' '.join(stmt_parts)
        if (
            ';' in code
            or (joined.count('(') == 0 and code.endswith(','))
            or (
                joined.count('(') > 0 and joined.count('(') == joined.count(')')
            )
        ):
            yield stmt_line, joined
            stmt_line = None
            stmt_parts = []
    if stmt_parts:
        yield stmt_line, ' '.join(stmt_parts)


def _ExtractEventConfigs(input_api, text):
    pattern = input_api.re.compile(r'\bEventConfig\s*\(')
    for match in pattern.finditer(text):
        prefix = text[: match.start()]
        depth = 1
        i = match.end()
        arg_start = i
        args = []
        while i < len(text) and depth > 0:
            if text[i] == '(':
                depth += 1
            elif text[i] == ')':
                depth -= 1
                if depth == 0:
                    args.append(text[arg_start:i].strip())
            elif text[i] == ',' and depth == 1:
                args.append(text[arg_start:i].strip())
                arg_start = i + 1
            i += 1
        if len(args) == 4:
            yield prefix, args


def _CheckNoComparatorAny(input_api, output_api):
    """Checks that no code uses Comparator(ANY, ...) directly."""
    results = []
    pattern = input_api.re.compile(_COMPARATOR_ANY_CALL_PATTERN)

    for f in _IterAffectedCppFiles(input_api):
        local_path = f.LocalPath()
        for line_num, line in f.ChangedContents():
            if pattern.search(line):
                message = (
                    f'{local_path}:{line_num}: Do not use Comparator(ANY, ...). '
                    f'Use kAlwaysTrue (or kAlwaysAvailable / kNoRestrictions) instead.'
                )
                results.append(output_api.PresubmitError(message))

    return results


def _CheckNoRedundantNamespaceQualifier(input_api, output_api):
    """Checks that C++ files do not use redundant 'feature_engagement::' qualifiers."""
    results = []
    namespace_decl_pattern = input_api.re.compile(
        r'^\s*namespace\s+feature_engagement\b'
    )

    for f in input_api.AffectedFiles():
        local_path = f.LocalPath()
        if not local_path.endswith(('.cc', '.h', '.mm', '.cpp')):
            continue

        for line_num, line in f.ChangedContents():
            # Strip single-line comments
            stripped = line.split('//')[0].strip()
            # Skip comment-only lines in block comments
            if stripped.startswith('*') or stripped.startswith('/*'):
                continue
            # Skip namespace declarations (e.g. namespace feature_engagement::stats)
            if namespace_decl_pattern.match(stripped):
                continue

            if 'feature_engagement::' in stripped:
                message = (
                    f'{local_path}:{line_num}: Redundant "feature_engagement::" qualifier. '
                    f'Code in components/feature_engagement is already in the '
                    f'feature_engagement namespace. If this is a false positive, please '
                    f'ping mschillaci@.'
                )
                results.append(output_api.PresubmitPromptWarning(message))

    return results


def _CheckAlwaysTrueInUsedOrTrigger(input_api, output_api):
    """Warns when used or trigger EventConfig uses kAlwaysTrue or equivalent."""
    results = []
    any_comparator_pattern = input_api.re.compile(_ANY_COMPARATOR_PATTERN)
    assign_pattern = input_api.re.compile(r'(\w*(?:used|trigger)\w*)\s*=')

    for f in _IterAffectedCppFiles(input_api):
        local_path = f.LocalPath()
        for start_line, stmt in _IterChangedStatements(input_api, f):
            for prefix, args in _ExtractEventConfigs(input_api, stmt):
                if len(args) < 2:
                    continue
                field_match = assign_pattern.search(prefix)
                if not field_match:
                    continue
                field_name = field_match.group(1)
                comparator = args[1]
                if any_comparator_pattern.fullmatch(comparator):
                    results.append(
                        output_api.PresubmitPromptWarning(
                            f'{local_path}:{start_line}: `{field_name}` is configured '
                            f'with `{comparator}` (an unconstrained comparator). Setting '
                            f'used or trigger events to always evaluate to true is '
                            f'typically used for passive data recording only and '
                            f'prevents standard IPH frequency capping. This may be '
                            f'valid, but please double-check that this configuration is '
                            f'intentional. See '
                            f'https://chromium.googlesource.com/chromium/src/+/main/'
                            f'components/feature_engagement/README.md#comparator'
                        )
                    )

    return results


def _CheckEventConfigComparatorAndWindow(input_api, output_api):
    """Checks that EventConfig uses an ANY comparator iff window is 0."""
    results = []
    any_comp_re = input_api.re.compile(_ANY_COMPARATOR_PATTERN)

    for f in _IterAffectedCppFiles(input_api):
        local_path = f.LocalPath()
        for line_num, stmt in _IterChangedStatements(input_api, f):
            for _, args in _ExtractEventConfigs(input_api, stmt):
                comparator = args[1]
                window = args[2]
                is_any = bool(any_comp_re.fullmatch(comparator))
                is_zero_window = bool(input_api.re.fullmatch(r'0[uU]?', window))
                if is_any != is_zero_window:
                    message = (
                        f'{local_path}:{line_num}: EventConfig must use an ANY '
                        f'comparator (e.g. kAlwaysTrue) if and only if window is 0 '
                        f'(found comparator `{comparator}` with window `{window}`). '
                        f'See https://chromium.googlesource.com/chromium/src/+/main/'
                        f'components/feature_engagement/README.md#comparator for details.'
                    )
                    results.append(output_api.PresubmitError(message))

    return results


def _CheckNewFlagsHaveMetrics(input_api, output_api):
    """Checks that adding new feature flags also updates actions.xml and histograms.xml."""
    flag_patterns = {
        _FEATURE_CONSTANTS_H_PATH: input_api.re.compile(
            r'\b(?:FEATURE_CONSTANTS_DECLARE_FEATURE|BASE_DECLARE_FEATURE)\s*\(\s*(k\w+)'
        ),
        _FEATURE_CONSTANTS_CC_PATH: input_api.re.compile(
            r'\bBASE_FEATURE\s*\(\s*(k\w+)'
        ),
        _FEATURE_LIST_H_PATH: input_api.re.compile(
            r'\b(?:DEFINE_VARIATION_PARAM|VARIATION_ENTRY)\s*\(\s*(k\w+)'
        ),
        _FEATURE_LIST_CC_PATH: input_api.re.compile(r'^\s*&\s*\b(k\w+)'),
    }

    added_flags = set()
    for f in input_api.AffectedFiles():
        local_path = f.LocalPath()
        pattern = flag_patterns.get(local_path)
        if not pattern:
            continue

        old_contents_text = '\n'.join(f.OldContents())
        for _, stmt in _IterChangedStatements(input_api, f):
            for match in pattern.finditer(stmt):
                flag_name = match.group(1)
                if flag_name == 'kIPHDummyFeature':
                    continue
                # Skip if the flag already existed anywhere in the file before this CL.
                if input_api.re.search(
                    r'\b' + input_api.re.escape(flag_name) + r'\b',
                    old_contents_text,
                ):
                    continue
                added_flags.add(flag_name)

    if not added_flags:
        return []

    affected_paths = {
        f.UnixLocalPath() for f in input_api.change.AffectedFiles()
    }
    missing_files = []
    if _ACTIONS_XML_PATH not in affected_paths:
        missing_files.append(_ACTIONS_XML_PATH)
    if _HISTOGRAMS_XML_PATH not in affected_paths:
        missing_files.append(_HISTOGRAMS_XML_PATH)

    if not missing_files:
        return []

    flag_list_str = ', '.join(sorted(added_flags))
    missing_files_str = '\n  - '.join(missing_files)
    message = (
        f'It looks like you are adding one or more new feature flags '
        f'({flag_list_str}), but the following required metric files are missing '
        f'from this change:\n'
        f'  - {missing_files_str}\n\n'
        f'Whenever a new Feature Engagement (IPH) flag is added, you must also '
        f'update the corresponding entries in actions.xml and histograms.xml.\n\n'
        f'For details on the required code and UMA changes, see:\n'
        f'https://chromium.googlesource.com/chromium/src/+/main/components/feature_engagement/README.md#required-code-changes\n\n'
        f'Tip: You can use the helper script `python3 tools/feature_engagement/generate_iph_entry.py` '
        f'to generate all required boilerplate code, entries, and UMA configurations automatically.'
    )
    return [output_api.PresubmitError(message)]


def _HasValidTag(input_api, tag_re):
    """Checks whether the CL description matches the given tag regex."""
    description = ''
    if hasattr(input_api, 'change') and hasattr(
        input_api.change, 'DescriptionText'
    ):
        description = input_api.change.DescriptionText() or ''
    compiled_re = input_api.re.compile(tag_re, input_api.re.MULTILINE)
    return bool(compiled_re.search(description))


def _IsCommitting(input_api):
    """True for a real commit (CQ submit), False for uploads and dry runs."""
    return bool(
        getattr(input_api, 'is_committing', False)
        and not getattr(input_api, 'dry_run', False)
    )


def _HasUserEducationJavaPolicyOwnerApproval(input_api):
    """Checks whether a User Education Java policy owner approved the CL.

    Mirrors _ChangeHasReviewerFromOwnersFile in the root PRESUBMIT.py. With
    no Gerrit issue there is nothing to look up, so the CL is let through. A
    policy owner authoring the CL, a Code-Review+1 from a policy owner, or
    Owners-Override all satisfy the check.
    """
    issue = getattr(input_api.change, 'issue', None)
    gerrit = getattr(input_api, 'gerrit', None)
    canned_checks = getattr(input_api, 'canned_checks', None)
    if not issue or gerrit is None or canned_checks is None:
        return True

    if hasattr(gerrit, 'IsOwnersOverrideApproved') and (
        gerrit.IsOwnersOverrideApproved(issue)
    ):
        return True

    owner_email, approvers = canned_checks.GetCodereviewOwnerAndReviewers(
        input_api, None, approval_needed=True
    )
    if owner_email in _USER_EDUCATION_JAVA_POLICY_OWNERS:
        return True
    return any(
        owner in approvers for owner in _USER_EDUCATION_JAVA_POLICY_OWNERS
    )


def _PolicyBypassReviewResults(input_api, output_api):
    """Warns on upload/dry run; blocks commit until a policy owner approves."""
    owners = ', '.join(_USER_EDUCATION_JAVA_POLICY_OWNERS)
    if not _IsCommitting(input_api):
        return [
            output_api.PresubmitPromptWarning(
                f'Enterprise policy bypass change detected. Make sure the CL '
                f'description includes '
                f'{_BYPASS_POLICY_TAG}=<justification and link to approval> '
                f'and that a User Education (Java) owner approves this change '
                f'(Code-Review+1) before it is submitted.'
            )
        ]
    if _HasUserEducationJavaPolicyOwnerApproval(input_api):
        return []
    return [
        output_api.PresubmitError(
            f'Enterprise policy bypass change detected '
            f'({_BYPASS_POLICY_TAG} is set), but no User Education (Java) '
            f'policy owner has approved this change. Get a Code-Review+1 '
            f'from a User Education (Java) policy owner (possible owners: '
            f'{owners}) before submitting.'
        )
    ]


def _CheckEnterprisePolicyBypass(input_api, output_api):
    """Enforces allowlist and CL tag rules for bypassing PromotionsEnabled."""
    results = []
    feature_re = input_api.re.compile(r'\bFeatureConstants\.([A-Z0-9_]+)\b')
    has_bypass_tag = _HasValidTag(input_api, _BYPASS_POLICY_TAG_RE)
    owners = ', '.join(_USER_EDUCATION_JAVA_POLICY_OWNERS)

    # 1. Check modifications to EnterprisePolicyBypassAllowlist.java.
    for f in input_api.AffectedFiles():
        if f.UnixLocalPath() != _ENTERPRISE_POLICY_ALLOWLIST_PATH:
            continue

        new_text = '\n'.join(f.NewContents())
        old_text = '\n'.join(f.OldContents())

        # Block unconditional return true in EnterprisePolicyBypassAllowlist.
        if input_api.re.search(r'\breturn\s+true\s*;', new_text):
            results.append(
                output_api.PresubmitError(
                    f'{_ENTERPRISE_POLICY_ALLOWLIST_PATH}: Do not return true '
                    f'unconditionally from '
                    f'isAllowedToBypassEnterprisePolicy(). Features must be '
                    f'explicitly listed in ALLOWLISTED_FEATURES.'
                )
            )

        old_features = set(feature_re.findall(old_text))
        new_features = set(feature_re.findall(new_text))
        added_features = sorted(new_features - old_features)

        # Only enforce the tag when an existing allowlist is modified. On
        # initial creation (no old contents) the whole file is reviewed by the
        # CL that introduces it, and stacked CLs diffed against origin/main
        # would otherwise see every entry as newly added.
        if old_text and added_features and not has_bypass_tag:
            formatted = '\n'.join(
                f'  - FeatureConstants.{name}' for name in added_features
            )
            results.append(
                output_api.PresubmitError(
                    f'You are adding one or more features to '
                    f'{_ENTERPRISE_POLICY_ALLOWLIST_PATH} to bypass the '
                    f'PromotionsEnabled enterprise policy:\n'
                    f'{formatted}\n\n'
                    f'Bypassing the PromotionsEnabled enterprise policy is '
                    f'restricted to critical legal, regulatory, privacy, or '
                    f'security prompts and requires explicit approval from a '
                    f'User Education (Java) policy owner (possible owners: '
                    f'{owners}).\n\n'
                    f'Add one of them as a reviewer and include the following '
                    f'line in your CL description with a justification and '
                    f'link to the approval or bug:\n'
                    f'  {_BYPASS_POLICY_TAG}='
                    f'<justification and link to approval>'
                )
            )

    # 2. Check modifications to TrackerImpl.java that alter policy enforcement.
    for f in input_api.AffectedFiles():
        if f.UnixLocalPath() != _TRACKER_IMPL_JAVA_PATH:
            continue

        new_text = '\n'.join(f.NewContents())
        old_text = '\n'.join(f.OldContents())
        if (
            old_text
            and 'isAllowedToBypassEnterprisePolicy' in old_text
            and 'isAllowedToBypassEnterprisePolicy' not in new_text
            and not has_bypass_tag
        ):
            results.append(
                output_api.PresubmitError(
                    f'{_TRACKER_IMPL_JAVA_PATH}: Removing the '
                    f'isAllowedToBypassEnterprisePolicy() check from '
                    f'TrackerImpl is not permitted without approval from a '
                    f'User Education (Java) policy owner (possible owners: '
                    f'{owners}) and `{_BYPASS_POLICY_TAG}=<justification>` '
                    f'in the CL description.'
                )
            )

    if has_bypass_tag:
        results.extend(_PolicyBypassReviewResults(input_api, output_api))
    return results


def _CheckUiFormatAdditions(input_api, output_api):
    """Requires new @UiFormat constants to be mapped, exhaustive and justified.

    All three switches over @UiFormat (formatToString in FeatureProperties,
    getCooldownFeature and getCooldownTriggerEvent in TrackerImpl) have a
    default branch, so the compiler will not notice a new format that falls
    outside every cooldown group. A new constant therefore has to be listed
    in the @IntDef, handled in all three switches in the same CL, and
    justified in the CL description.
    """
    results = []
    constant_re = input_api.re.compile(
        r'^\s*int\s+([A-Z][A-Z0-9_]*)\s*=\s*\d+\s*;', input_api.re.MULTILINE
    )

    added_formats = []
    new_ui_format_text = ''
    for f in input_api.AffectedFiles():
        if f.UnixLocalPath() != _UI_FORMAT_JAVA_PATH:
            continue
        new_ui_format_text = '\n'.join(f.NewContents())
        old_text = '\n'.join(f.OldContents())
        # Skip initial creation; the whole file is reviewed by that CL.
        if not old_text:
            continue
        old_formats = set(constant_re.findall(old_text))
        new_formats = set(constant_re.findall(new_ui_format_text))
        added_formats = sorted(new_formats - old_formats)

    if not added_formats:
        return results

    names = ', '.join(added_formats)
    new_contents_by_path = {
        f.UnixLocalPath(): '\n'.join(f.NewContents())
        for f in input_api.AffectedFiles()
    }
    intdef_match = input_api.re.search(
        r'@IntDef\(\{(.*?)\}\)', new_ui_format_text, input_api.re.DOTALL
    )
    intdef_text = intdef_match.group(1) if intdef_match else None
    # (path, label, minimum number of `case UiFormat.<NAME>:` labels).
    required_switches = (
        (_FEATURE_PROPERTIES_JAVA_PATH, 'FeatureProperties.formatToString()', 1),
        (
            _TRACKER_IMPL_JAVA_PATH,
            'TrackerImpl.getCooldownFeature() and '
            'TrackerImpl.getCooldownTriggerEvent()',
            2,
        ),
    )

    missing = []
    for name in added_formats:
        if intdef_text is not None and f'UiFormat.{name}' not in intdef_text:
            missing.append(f'UiFormat.{name} is missing from the @IntDef list')
        case_re = input_api.re.compile(r'\bcase\s+UiFormat\.' + name + r'\s*:')
        for path, label, min_count in required_switches:
            count = len(case_re.findall(new_contents_by_path.get(path, '')))
            if count < min_count:
                missing.append(
                    f'{label} must handle UiFormat.{name} '
                    f'({min_count} case label(s) expected in {path}, '
                    f'found {count})'
                )

    if missing:
        formatted = '\n'.join(f'  - {item}' for item in missing)
        results.append(
            output_api.PresubmitError(
                f'You are adding new @UiFormat constants ({names}) to '
                f'{_UI_FORMAT_JAVA_PATH} but the following required updates '
                f'are missing:\n'
                f'{formatted}\n\n'
                f'Every UI format must be listed in the @IntDef, mapped to a '
                f'cooldown group (or explicitly to none) in TrackerImpl, and '
                f'given a name in FeatureProperties so the cooldown rules and '
                f'logging stay exhaustive.'
            )
        )

    if not _HasValidTag(input_api, _NEW_UI_FORMAT_TAG_RE):
        results.append(
            output_api.PresubmitError(
                f'Adding a new @UiFormat ({names}) requires an explanation of '
                f'what the new surface is and how it participates in the '
                f'interruptive or blue bubble cooldown groups. Include the '
                f'following line in your CL description:\n'
                f'  {_NEW_UI_FORMAT_TAG}=<justification and cooldown behavior>'
            )
        )
    return results


def _CheckNoUnauthorizedTrackerJni(input_api, output_api):
    """Blocks unauthorized JNI bridges to feature_engagement::Tracker."""
    results = []
    jni_pattern = input_api.re.compile(
        r'(?:@NativeMethods|@CalledByNative|\bDEFINE_JNI\b)'
    )
    tracker_cpp_pattern = input_api.re.compile(
        r'\b(?:feature_engagement::Tracker|TrackerImplAndroid)\b'
    )

    for f in input_api.change.AffectedFiles():
        unix_path = f.UnixLocalPath()
        if unix_path in _ALLOWED_JNI_FILES:
            continue
        if unix_path.endswith(
            (
                'Test.java',
                'Tests.java',
                '_unittest.cc',
                '_unittest.h',
                '_browsertest.cc',
            )
        ):
            continue

        if unix_path.endswith(('.java', '.cc', '.h')) and (
            unix_path.startswith('components/feature_engagement/')
            or tracker_cpp_pattern.search('\n'.join(f.NewContents()))
        ):
            for line_num, line in f.ChangedContents():
                stripped = line.split('//', 1)[0].strip()
                if jni_pattern.search(stripped):
                    results.append(
                        output_api.PresubmitError(
                            f'{f.LocalPath()}:{line_num}: Unauthorized JNI '
                            f'bridge in Feature Engagement. All Java calls to '
                            f'the Feature Engagement Tracker must go through '
                            f'TrackerImpl.java so enterprise policy and '
                            f'cooldown group rules are enforced.'
                        )
                    )
                    break

    return results
