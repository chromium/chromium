---
name: signin/prefer-get-accounts-with-refresh-tokens-over-cookie-jar
rubric: correctness
severity: should_fix
---

**Rule.** When code needs a list of accounts from `signin::IdentityManager`,
prefer `IdentityManager::GetAccountsWithRefreshTokens()` (or
`IdentityManager::GetExtendedAccountInfoForAccountsWithRefreshToken()`) over
`IdentityManager::GetAccountsInCookieJar()`, unless the feature specifically
inspects or manages Gaia web cookies.

**Why.** `GetAccountsWithRefreshTokens()` returns the authoritative list of
accounts known to Chrome (backed by OAuth2 refresh tokens or platform account
managers), whereas `GetAccountsInCookieJar()` reflects web cookie jar state that
can be cleared by the user, become stale, diverge from browser sign-in state, or
trigger asynchronous network requests.

## Examples

Bad:

```cpp
std::vector<GaiaId> GetAvailableAccountGaiaIds(
    const signin::IdentityManager* identity_manager) {
  std::vector<GaiaId> gaia_ids;
  signin::AccountsInCookieJarInfo cookie_info =
      identity_manager->GetAccountsInCookieJar();
  for (const gaia::ListedAccount& account :
       cookie_info.GetValidSignedInAccounts()) {
    gaia_ids.push_back(account.gaia_id);
  }
  return gaia_ids;
}
```

Good:

```cpp
std::vector<GaiaId> GetAvailableAccountGaiaIds(
    const signin::IdentityManager* identity_manager) {
  std::vector<GaiaId> gaia_ids;
  for (const CoreAccountInfo& account :
       identity_manager->GetAccountsWithRefreshTokens()) {
    gaia_ids.push_back(account.gaia);
  }
  return gaia_ids;
}
```
