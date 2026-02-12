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

Filters are declared in C++ test code. The most convenient way is `SetFilters()`,
which accepts a multi-line string of platform-prefixed directives:

```cpp
IN_PROC_BROWSER_TEST_P(MyTest, FocusOnly) {
  SetFilters(R"(
@AURALINUX-ALLOW:focus-event*
@MAC-ALLOW:AXFocusedUIElementChanged*
@UIA-WIN-ALLOW:AutomationFocusChanged*
@WIN-ALLOW:EVENT_OBJECT_FOCUS*
)");
  my_view_->RequestFocus();
  EXPECT_TRUE(EndTestAndCompareEvents("focus-only"));
}
```

Supported platform prefixes: `@MAC-`, `@WIN-`, `@UIA-WIN-`, `@AURALINUX-`.
Supported directives: `ALLOW`, `DENY`, `ALLOW-EMPTY`.

You can also use `AddAllowFilter()`, `AddDenyFilter()`, or the lower-level
`AddPropertyFilter()` for single-platform or cross-platform filters.

## Skip marker

Add `#<skip>` to an expectation file to skip the test for that platform.

