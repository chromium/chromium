---
name: autofill/avoid-default-in-switch-over-autofill-enums
rubric: correctness
severity: should_fix
---

**Rule.** Write `switch` statements over Autofill enums such as `FieldType`,
`SuggestionType` or `FillingProduct` exhaustively and without a `default:`
label; likewise prefer an exhaustive `switch` (or a helper containing one) over
an `||` chain of equality comparisons when classifying enum values. When every
`case` returns from a non-`void` function, place `NOTREACHED();` after the
`switch` statement (never inside a `default:` label) so out-of-range values do
not fall off the end of the function. Only values deserialized from disk, a
database or an integer cast may need an explicit out-of-range recovery path
after all enumerators are listed.

**Why.** Autofill enums grow frequently. Without `default:` the compiler's
`-Wswitch` reports every place that must be updated when an enumerator is added;
a `default:` or an `||` chain hides that signal and leaves new types silently
misclassified, which typically surfaces as wrong filling or missing suggestions
much later.

## Examples

Bad:

```cpp
// The default label and the || chain both hide newly added FieldTypes.
bool IsNameType(FieldType type) {
  switch (type) {
    case NAME_FIRST:
    case NAME_LAST:
      return true;
    default:
      return false;
  }
}
bool IsNameTypeToo(FieldType type) {
  return type == NAME_FIRST || type == NAME_LAST || type == NAME_MIDDLE;
}
```

Good:

```cpp
// Exhaustive switch: adding a FieldType fails to compile until handled here.
bool IsNameType(FieldType type) {
  switch (type) {
    case NAME_FIRST:
    case NAME_MIDDLE:
    case NAME_LAST:
      return true;
    case EMAIL_ADDRESS:
    case PHONE_HOME_WHOLE_NUMBER:
    // ... one case per remaining FieldType enumerator, no default.
      return false;
  }
  NOTREACHED();
}
```
