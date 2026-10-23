---
name: signin/do-not-use-deprecated-consent-level-ksync
rubric: design
severity: should_fix
---

**Rule.** `signin::ConsentLevel::kSync` is deprecated; do not use it in new code
unless calling an existing legacy API that explicitly requires it. Use
`signin::ConsentLevel::kSignin` instead.

**Why.** `signin::ConsentLevel::kSync` is deprecated and being removed across
platforms (see `components/signin/public/base/consent_level.h`). Introducing new
usages of `ConsentLevel::kSync` blocks the migration away from `kSync` and
breaks features for signed-in users who do not have legacy browser sync consent.

## Examples

Bad:

```cpp
if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  StartFeatureForAccount(account.gaia);
}
```

Good:

```cpp
if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  StartFeatureForAccount(account.gaia);
}
```
