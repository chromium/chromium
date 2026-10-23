---
name: signin/prefer-cpp-over-java-or-objc-for-shared-logic
rubric: design
severity: should_fix
---

**Rule.** For shared sign-in logic and helpers (such as eligibility checks,
account state evaluation, pref management, and metrics helpers), prefer
implementing them in C++ (e.g., under `components/signin/`) instead of
Objective-C or Java so that the implementation can be shared across platforms.
Expose thin JNI or Objective-C++ wrappers only when calling from platform UI
code.

**Why.** Implementing non-UI sign-in logic and helpers separately in Java or
Objective-C duplicates business rules across Android, iOS, and Desktop, leading
to subtle behavioral divergence and duplicated test maintenance. Keeping shared
logic in C++ provides a single source of truth across all platforms.

## Examples

Bad:

```java
// Duplicating cross-platform account capability / eligibility logic in Java:
public static boolean canShowPromoForAccount(
        AccountInfo accountInfo, PrefService prefs) {
    if (prefs.getInteger(Pref.PROMO_IMPRESSION_COUNT) >= MAX_IMPRESSIONS) {
        return false;
    }
    return accountInfo
                    .getAccountCapabilities()
                    .canShowHistorySyncOptInsWithoutMinorModeRestrictions()
            == Tribool.TRUE;
}
```

Good:

```cpp
// Implemented once in C++ (components/signin/) and exposed to Java/ObjC if
// needed:
bool CanShowPromoForAccount(const AccountInfo& account_info,
                            const PrefService& prefs) {
  if (prefs.GetInteger(prefs::kPromoImpressionCount) >= kMaxImpressions) {
    return false;
  }
  return account_info.GetAccountCapabilities()
             .can_show_history_sync_opt_ins_without_minor_mode_restrictions() ==
         signin::Tribool::kTrue;
}
```
