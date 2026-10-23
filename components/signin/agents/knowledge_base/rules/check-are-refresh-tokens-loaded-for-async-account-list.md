---
name: signin/check-are-refresh-tokens-loaded-for-async-account-list
rubric: correctness
severity: should_fix
---

**Rule.** Because `signin::IdentityManager` loads the list of accounts and
refresh tokens asynchronously during startup, any code that queries refresh
token state or the account list (such as `GetAccountsWithRefreshTokens()`,
`GetExtendedAccountInfoForAccountsWithRefreshToken()`,
`HasAccountWithRefreshToken()`, `HasPrimaryAccountWithRefreshToken()`, or
`GetErrorStateOfRefreshTokenForAccount()`) and can run before tokens finish
loading must check `IdentityManager::AreRefreshTokensLoaded()` first and listen
to `IdentityManager::Observer::OnRefreshTokensLoaded()` if needed. Note that
primary account identity queries (`GetPrimaryAccountInfo()`,
`GetPrimaryAccountId()`, `HasPrimaryAccount()`) are loaded synchronously from
prefs on startup and do not require `AreRefreshTokensLoaded()`.

**Why.** Before `AreRefreshTokensLoaded()` returns `true`,
`GetAccountsWithRefreshTokens()` and related token queries return empty or
false even when the user has valid accounts stored on disk, causing startup
callers to misclassify signed-in accounts as missing.

## Examples

Bad:

```cpp
void StartupAccountChecker::CheckAccounts() {
  // May return an empty list if called before refresh tokens are loaded!
  std::vector<CoreAccountInfo> accounts =
      identity_manager_->GetAccountsWithRefreshTokens();
  if (accounts.empty()) {
    ClearCachedFeatureData();
  }
}
```

Good:

```cpp
void StartupAccountChecker::CheckAccounts() {
  if (!identity_manager_->AreRefreshTokensLoaded()) {
    identity_manager_observation_.Observe(identity_manager_);
    return;
  }
  if (identity_manager_->GetAccountsWithRefreshTokens().empty()) {
    ClearCachedFeatureData();
  }
}

void StartupAccountChecker::OnRefreshTokensLoaded() {
  identity_manager_observation_.Reset();
  CheckAccounts();
}
```
