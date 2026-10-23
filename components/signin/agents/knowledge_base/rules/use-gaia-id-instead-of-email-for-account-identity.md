---
name: signin/use-gaia-id-instead-of-email-for-account-identity
rubric: correctness
severity: should_fix
---

**Rule.** Do not use email addresses (`CoreAccountInfo::email`,
`AccountInfo::GetEmail()`, or raw email strings) to identify, compare, store, or
look up accounts obtained from `signin::IdentityManager` (for example, avoid
`IdentityManager::FindExtendedAccountInfoByEmailAddress()` when identifying a
known account). Use `GaiaId` (`CoreAccountInfo::gaia`,
`AccountInfo::GetGaiaId()`, and
`IdentityManager::FindExtendedAccountInfoByGaiaId()`) instead. As an exception,
it is fine to use emails as identifiers when calling platform APIs that require
emails (which is often the case on Android). When comparing emails is
unavoidable, use `gaia::AreEmailsSame()` instead of raw string comparison.

**Why.** Email addresses are mutable (users can change their account email) and
have non-trivial canonicalization rules (casing, dots in `@gmail.com`, and
domain aliases like `@googlemail.com`), making raw string comparisons or
email-keyed storage error-prone. `GaiaId` is an immutable, strongly typed unique
identifier for a Google account in `signin::IdentityManager`.

## Examples

Bad:

```cpp
class AccountFeatureTracker {
 public:
  void StartTrackingPrimaryAccount(signin::IdentityManager* identity_manager) {
    CoreAccountInfo primary = identity_manager->GetPrimaryAccountInfo(
        signin::ConsentLevel::kSignin);
    tracked_email_ = primary.email;
  }

  AccountInfo GetTrackedAccountInfo(
      signin::IdentityManager* identity_manager) const {
    return identity_manager->FindExtendedAccountInfoByEmailAddress(
        tracked_email_);
  }

 private:
  std::string tracked_email_;
};
```

Good:

```cpp
class AccountFeatureTracker {
 public:
  void StartTrackingPrimaryAccount(signin::IdentityManager* identity_manager) {
    CoreAccountInfo primary = identity_manager->GetPrimaryAccountInfo(
        signin::ConsentLevel::kSignin);
    tracked_gaia_id_ = primary.gaia;
  }

  AccountInfo GetTrackedAccountInfo(
      signin::IdentityManager* identity_manager) const {
    return identity_manager->FindExtendedAccountInfoByGaiaId(tracked_gaia_id_);
  }

 private:
  GaiaId tracked_gaia_id_;
};
```
