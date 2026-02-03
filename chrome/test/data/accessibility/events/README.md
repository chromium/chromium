# Accessibility Events Test Data

This directory contains expectation files for Views accessibility event tests.

## File naming convention

Expectation files follow the naming pattern:
```
<test-name>-expected-<platform>.txt
```

Where `<platform>` is one of:
- `win` - Windows IAccessible2 (IA2)
- `uia-win` - Windows UI Automation (UIA)
- `mac` - macOS NSAccessibility
- `auralinux` - Linux ATK

## Creating new tests

1. Create your test by inheriting from `DumpAccessibilityEventsViewsTestBase`
2. Run the test with `--generate-accessibility-test-expectations` flag
3. Review the generated expectation files
4. Add the expectation files to source control

## Filtering

Expectation files can include filter directives at the top:

```
@WIN-ALLOW:EVENT_OBJECT_FOCUS*
@WIN-DENY:EVENT_OBJECT_LOCATIONCHANGE*
```

## Skip marker

Add `#<skip>` to an expectation file to skip the test for that platform.
