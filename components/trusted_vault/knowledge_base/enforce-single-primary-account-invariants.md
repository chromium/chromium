---
name: trusted_vault/enforce-single-primary-account-invariants
rubric: correctness
severity: blocking
---

**Rule.** Bind primary-account-scoped collaborators (such as
`LocalRecoveryFactor` implementations and
`TrustedVaultDegradedRecoverabilityHandler`) to a mandatory `CoreAccountInfo` at
construction time and manage their in-flight requests via RAII. In
`StandaloneTrustedVaultBackend`, validate `primary_account_` and early-return
preconditions before enqueuing callbacks into `ongoing_*` containers, verify
`gaia_id == primary_account_->gaia` before draining deferred queues with
`std::exchange` (evicting other accounts via `std::erase_if` when deferring),
and when `primary_account_` changes in `SetPrimaryAccount()`, immediately tear
down primary-account-scoped state and fulfill pending key fetches with an
"abort" result.

**Why.** Callers may invoke backend operations before `SetPrimaryAccount()` or
across primary account transitions. Enqueuing into `ongoing_*` before validating
the primary account, draining deferred queues before checking that the queued
account matches `primary_account_`, or resetting `SetPrimaryAccount()` state
without aborting in-flight work and fulfilling pending `ongoing_fetch_keys_`
callbacks causes cross-account state corruption or permanently dropped callbacks.

## Examples

Bad:

```cpp
void StandaloneTrustedVaultBackend::FetchKeys(
    const CoreAccountInfo& account_info,
    SecurityDomainId security_domain,
    FetchKeysCallback callback) {
  // BAD: Enqueues callback before validating primary_account_ and early returns.
  ongoing_fetch_keys_[security_domain].callbacks.push_back(std::move(callback));
  if (!primary_account_.has_value() ||
      primary_account_->gaia != account_info.gaia) {
    return;
  }
  AttemptRecoveryFactor(security_domain, 0);
}

void StandaloneTrustedVaultBackend::MaybeProcessPendingTrustedRecoveryMethods() {
  // BAD: Drains deferred queue before verifying that queued entries belong to
  // the current primary_account_, silently dropping entries if they do not match.
  auto methods = std::exchange(pending_trusted_recovery_methods_, {});
  for (auto& method : methods) {
    if (primary_account_.has_value() &&
        method.gaia_id == primary_account_->gaia) {
      AddTrustedRecoveryMethod(method.gaia_id, method.security_domain,
                               method.public_key, method.method_type_hint,
                               std::move(method.completion_callback));
    }
  }
}
```

Good:

```cpp
void StandaloneTrustedVaultBackend::FetchKeys(
    const CoreAccountInfo& account_info,
    SecurityDomainId security_domain,
    FetchKeysCallback callback) {
  if (!primary_account_.has_value() ||
      primary_account_->gaia != account_info.gaia) {
    FulfillFetchKeys(account_info.gaia, security_domain, std::move(callback),
                     TrustedVaultRecoverKeysOutcomeForUMA::kNoPrimaryAccount);
    return;
  }
  // GOOD: Validated preconditions before queueing into ongoing_fetch_keys_.
  auto& ongoing_fetch = ongoing_fetch_keys_[security_domain];
  ongoing_fetch.gaia_id = account_info.gaia;
  ongoing_fetch.callbacks.push_back(std::move(callback));
  AttemptRecoveryFactor(security_domain, 0);
}

void StandaloneTrustedVaultBackend::MaybeProcessPendingTrustedRecoveryMethods() {
  // GOOD: Verifies primary_account_ match before draining the deferred queue.
  if (!primary_account_.has_value() ||
      refresh_token_error_state_ ==
          RefreshTokenErrorState::kPersistentAuthError ||
      pending_trusted_recovery_methods_.empty() ||
      pending_trusted_recovery_methods_[0].gaia_id != primary_account_->gaia) {
    return;
  }
  auto methods = std::exchange(pending_trusted_recovery_methods_, {});
  for (auto& method : methods) {
    CHECK_EQ(method.gaia_id, primary_account_->gaia);
    AddTrustedRecoveryMethod(method.gaia_id, method.security_domain,
                             method.public_key, method.method_type_hint,
                             std::move(method.completion_callback));
  }
}
```
