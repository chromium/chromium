---
name: signin/document-privacy-approval-when-storing-pii-in-prefs
rubric: safety
severity: blocking
---

**Rule.** Storing Personally Identifiable Information (PII) (such as email
addresses, `GaiaId`, or account names) in prefs (`PrefService` or
`local_state`) requires Privacy review. Where a new PII-storing pref is
registered or written, include a comment with a link to the bug or design doc
tracking the Privacy review, so reviewers can follow up if they have questions
(call sites of existing, already-reviewed prefs do not need to repeat the
comment). Privacy approval does not need to be obtained before landing the
implementation, but it must be obtained before the feature is launched or
experimented with on Beta or Stable.

**Why.** Persisting PII in preferences creates on-disk user data that must
comply with privacy, retention, and sign-out/profile-deletion cleanup policies.
Linking to the tracking bug or design doc lets reviewers verify that Privacy
review is tracked and understand why the PII persistence is required.

## Examples

Bad:

```cpp
void SaveLastSignedInAccount(PrefService* prefs,
                             const CoreAccountInfo& account) {
  prefs->SetString(prefs::kLastSignedInAccountGaiaId, account.gaia.ToString());
}
```

Good:

```cpp
void SaveLastSignedInAccount(PrefService* prefs,
                             const CoreAccountInfo& account) {
  // Privacy review for storing GaiaId in prefs: crbug.com/123456789
  // (cleared on sign-out and profile deletion).
  prefs->SetString(prefs::kLastSignedInAccountGaiaId, account.gaia.ToString());
}
```
