---
name: trusted_vault/record-uma-histograms-once-per-operation
rubric: correctness
severity: should_fix
---

**Rule.** Record Trusted Vault UMA outcome and status histograms once per
logical operation rather than inside candidate-key or member iteration loops.
Scope histograms by `{SecurityDomainId}` (via `GetSecurityDomainNameForUma`)
only when the operation clearly operates on a single security domain; operations
that are not associated with a security domain or that span multiple/all
security domains (such as `TrustedVault.FileReadStatus` or
`TrustedVault.RecoveryKeyStoreURLFetchResponse`) should not be scoped to a
single `SecurityDomainId`. Keep `*ForUMA` C++ enums (`LINT.IfChange` /
`LINT.ThenChange`) and histogram name suffixes synchronized with
`tools/metrics/histograms/metadata/trusted_vault/enums.xml` and
`tools/metrics/histograms/metadata/trusted_vault/histograms.xml`.

**Why.** Emitting metrics inside candidate-key loops records multiple samples
for a single logical operation and skews failure counts when a user has multiple
domain members. Artificially scoping cross-domain or domain-agnostic operations
to a single security domain misrepresents the operation's scope, and modifying
`*ForUMA` enums or suffix mappings without updating `enums.xml` and
`histograms.xml` breaks UMA dashboard analysis.

## Examples

Bad:

```cpp
// BAD: Records failure histogram on every non-matching or corrupted candidate
// key inside a loop instead of once per logical recovery operation.
for (const VaultMember& recovery_key : result.icloud_keys) {
  if (recovery_key.member_keys.empty()) {
    RecordTrustedVaultDownloadKeysStatus(
        LocalRecoveryFactorType::kICloudKeychain, security_domain_id,
        TrustedVaultDownloadKeysStatusForUMA::kMembershipEmpty);
    continue;
  }
}
```

Good:

```cpp
// GOOD: Tracks the most relevant failure status across candidate keys and
// records the outcome histogram once when the operation finishes.
TrustedVaultDownloadKeysStatusForUMA last_status =
    TrustedVaultDownloadKeysStatusForUMA::kNoNewKeys;
for (const VaultMember& recovery_key : result.icloud_keys) {
  if (recovery_key.member_keys.empty()) {
    last_status = TrustedVaultDownloadKeysStatusForUMA::kMembershipEmpty;
    continue;
  }
  // ...
}
FulfillOngoingRecoveryWithFailure(last_status);
```
