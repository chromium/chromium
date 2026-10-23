---
name: signin/do-not-log-account-pii
rubric: safety
severity: blocking
---

**Rule.** Do not log Personally Identifiable Information (PII) such as email
addresses (`CoreAccountInfo::email`, `AccountInfo::GetEmail()`) or `GaiaId`
(`CoreAccountInfo::gaia`, `AccountInfo::GetGaiaId()`, `CoreAccountId`) in
logging statements (`LOG`, `DLOG`, `VLOG`, `DVLOG`, Java `Log.*`, Objective-C
`NSLog`), crash keys (`SCOPED_CRASH_KEY*`, `base::debug::ScopedCrashKeyString`),
or trace events. Log only non-identifying state such as boolean presence,
account counts, or error codes.

**Why.** User email addresses and Gaia IDs are Personally Identifiable
Information (PII). Writing them to logs, crash reports, or traces risks leaking
sensitive user identity data into diagnostic outputs, feedback reports, and
crash uploads.

## Examples

Bad:

```cpp
void OnRefreshTokenUpdatedForAccount(const CoreAccountInfo& account_info) {
  VLOG(1) << "Refresh token updated for email=" << account_info.email
          << " gaia=" << account_info.gaia;
}
```

Good:

```cpp
void OnRefreshTokenUpdatedForAccount(const CoreAccountInfo& account_info) {
  VLOG(1) << "Refresh token updated, is_primary="
          << (account_info.gaia ==
              identity_manager_
                  ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                  .gaia);
}
```
