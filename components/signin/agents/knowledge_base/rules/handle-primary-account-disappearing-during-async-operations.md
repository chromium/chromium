---
name: signin/handle-primary-account-disappearing-during-async-operations
rubric: correctness
severity: should_fix
---

**Rule.** Never assume the primary account in `signin::IdentityManager` remains
present or unchanged across asynchronous operations (such as network requests,
access token fetches, `PostTask` callbacks, or user dialogs). Re-verify
`IdentityManager::HasPrimaryAccount()` or check that
`IdentityManager::GetPrimaryAccountInfo()` (or Java
`IdentityManager.getPrimaryAccountInfo()`, which is `@Nullable`) still matches
the initiating `GaiaId` inside the asynchronous callback (or cancel in-flight
work on `IdentityManager::Observer::OnPrimaryAccountChanged()`) rather than
`CHECK`-ing, passing to `assumeNonNull()`, or unconditionally using the primary
account.

**Why.** Users or enterprise policies can sign out or switch the primary account
while an asynchronous operation is in flight. Assuming the primary account is
still present when an async callback executes causes `CHECK` failures, empty
account usage, or applying results to the wrong account.

## Examples

Bad:

```cpp
void AccountDataSyncer::StartFetch() {
  CHECK(identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  FetchRemoteData(base::BindOnce(&AccountDataSyncer::OnDataFetched,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void AccountDataSyncer::OnDataFetched(std::string payload) {
  // Primary account may have been cleared while FetchRemoteData() was in
  // flight!
  CoreAccountInfo primary = identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kSignin);
  CHECK(!primary.IsEmpty());
  SavePayloadForAccount(primary.gaia, std::move(payload));
}
```

Good:

```cpp
void AccountDataSyncer::StartFetch() {
  CoreAccountInfo primary = identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kSignin);
  if (primary.IsEmpty()) {
    return;
  }
  FetchRemoteData(base::BindOnce(&AccountDataSyncer::OnDataFetched,
                                 weak_ptr_factory_.GetWeakPtr(), primary.gaia));
}

void AccountDataSyncer::OnDataFetched(GaiaId request_gaia_id,
                                      std::string payload) {
  CoreAccountInfo current_primary = identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kSignin);
  if (current_primary.gaia != request_gaia_id) {
    return;
  }
  SavePayloadForAccount(current_primary.gaia, std::move(payload));
}
```
