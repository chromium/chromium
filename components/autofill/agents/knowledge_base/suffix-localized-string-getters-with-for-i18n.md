---
name: autofill/suffix-localized-string-getters-with-for-i18n
rubric: readability
severity: nit
---

**Rule.** Append `ForI18n` to data-model, type, or entity getters that return a
localized name or label from `l10n_util` (for example
`EntityType::GetNameForI18n()` or
`GetMemoryDataTypeNameForI18n(MemoryDataType)`). Use this suffix whenever the
return value could otherwise be mistaken for a stable internal identifier or
key. Do not apply it to UI, view, or dialog controller methods and helpers (such
as `GetTitle()`, `GetDescription()`, `GetAccessibleName()`, or
`GetDialogButtonLabel()`), or to virtual interface overrides.

**Why.** The suffix tells callers that the value is translated UI text that
varies by locale and must never be compared, persisted or used as a key. Without
it, localized strings can be mistaken for stable internal identifiers and leak
into logic paths.

## Examples

Bad:

```cpp
// Missing ForI18n suffix; callers cannot tell that the returned name is
// locale-dependent UI text rather than an internal identifier.
std::u16string EntityType::GetName() const {
  switch (name()) {
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_DRIVERS_LICENSE_ENTITY_NAME);
    case EntityTypeName::kPassport:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_PASSPORT_ENTITY_NAME);
  }
  NOTREACHED();
}
```

Good:

```cpp
// Suffixed with ForI18n to mark the return value as localized UI text.
std::u16string EntityType::GetNameForI18n() const {
  switch (name()) {
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_DRIVERS_LICENSE_ENTITY_NAME);
    case EntityTypeName::kPassport:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_PASSPORT_ENTITY_NAME);
  }
  NOTREACHED();
}
```
