---
name: trusted_vault/coalesce-storage-mutations
rubric: perf
severity: should_fix
---

**Rule.** Coalesce storage mutations and skip disk writes when no state changed:
only call `WriteToDisk()` when removals actually erased elements (e.g.,
`google::protobuf::erase_if(...) > 0`) or when a field's value actually changed,
and batch multi-step updates in a single method using a `should_write_data` flag
or a single `Mutate*` call rather than writing to disk inside a loop or on
no-op updates.

**Why.** Each `WriteToDisk()` call in `LocalDomainsStorage` serializes the
`LocalDomainsData` protobuf, encrypts it with `os_crypt_async::Encryptor`,
performs an atomic disk write via `base::ImportantFileWriter::WriteFileAtomically`,
and records file-write UMA histograms. Skipping no-op mutations and coalescing
multi-user updates avoids unnecessary disk churn and metric skew.

## Examples

Bad:

```cpp
void LocalDomainsStorage::RemoveUsers(
    base::FunctionRef<bool(const trusted_vault_pb::UserDomainData&)>
        predicate) {
  // BAD: Writes to disk unconditionally even when zero users matched.
  google::protobuf::erase_if(*data_->mutable_user(), predicate);
  WriteToDisk();
}

void LocalDomainsStorage::ClearDataForUnknownUsersOrMarkForDeletion(
    const base::flat_set<GaiaId>& known_gaia_ids,
    const std::optional<GaiaId>& primary_account_gaia_id) {
  if (primary_account_gaia_id.has_value() &&
      !known_gaia_ids.contains(*primary_account_gaia_id)) {
    GetOrCreateUser(*primary_account_gaia_id)
        ->set_should_delete_keys_when_non_primary(true);
    // BAD: Redundant intermediate write before erasing non-primary users.
    WriteToDisk();
  }
  google::protobuf::erase_if(*data_->mutable_user(), ...);
  WriteToDisk();
}
```

Good:

```cpp
void LocalDomainsStorage::RemoveUsers(
    base::FunctionRef<bool(const trusted_vault_pb::UserDomainData&)>
        predicate) {
  // GOOD: Only writes to disk when at least one user entry was removed.
  if (google::protobuf::erase_if(*data_->mutable_user(), predicate) > 0) {
    WriteToDisk();
  }
}

void LocalDomainsStorage::ClearDataForUnknownUsersOrMarkForDeletion(
    const base::flat_set<GaiaId>& known_gaia_ids,
    const std::optional<GaiaId>& primary_account_gaia_id) {
  // GOOD: Coalesces flag mutation and user removal into at most one disk write.
  bool should_write_data = false;
  if (primary_account_gaia_id.has_value() &&
      !known_gaia_ids.contains(*primary_account_gaia_id)) {
    trusted_vault_pb::UserDomainData* user =
        GetOrCreateUser(*primary_account_gaia_id);
    if (!user->should_delete_keys_when_non_primary()) {
      user->set_should_delete_keys_when_non_primary(true);
      should_write_data = true;
    }
  }
  if (google::protobuf::erase_if(*data_->mutable_user(), ...) > 0) {
    should_write_data = true;
  }
  if (should_write_data) {
    WriteToDisk();
  }
}
```
