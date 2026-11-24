---
name: components/autofill/core/browser/payments/encapsulate-storage-transformations-in-preference-helpers
description: When writing preference or storage helpers, encapsulate storage transformations like signin::GaiaIdHash inside the helper rather than requiring callers to pass them
rubric: design
severity: should_fix
---

**Rule.** Encapsulate storage and security implementation details (such as
hashing account identifiers into signin::GaiaIdHash or encoding storage keys)
inside preference and persistence helper functions rather than requiring callers
to perform that pre-work. Design helper APIs to accept high-level client
interfaces or domain identities (such as AutofillClient& or const
signin::IdentityManager\*) and perform necessary lookups and transformations
internally.

**Why.** Requiring callers to compute storage-specific transformations, like
hashing a Gaia ID into a GaiaIdHash before calling a preference setter or
getter, leaks internal persistence mechanisms into high-level business logic.
This pollutes caller call sites with unnecessary boilerplate, duplicates
conversion code across callers, and complicates unit testing by forcing callers
to mock low-level storage keys.

## Examples

Bad:

```cpp
// Bad: Exposes internal storage/privacy hashing details to callers.
void SetUserAcknowledgedWalletReminderNotice(
    PrefService* prefs,
    const signin::GaiaIdHash& gaia_id_hash);

bool HasUserAcknowledgedWalletReminderNotice(
    const PrefService* prefs,
    const signin::GaiaIdHash& gaia_id_hash);
```

Good:

```cpp
// Good: Encapsulates identity lookup and hashing inside the helper using context.
void SetUserAcknowledgedWalletReminderNotice(AutofillClient& client);
bool HasUserAcknowledgedWalletReminderNotice(AutofillClient& client);
```
