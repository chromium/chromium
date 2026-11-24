---
name: components/autofill/core/browser/payments/validate-preconditions-before-feature-flag-check
description: When ordering feature checks, validate safe preconditions before FeatureList::IsEnabled to avoid premature enrollment, but evaluate flags before new or crashing code for kill-switch protection
rubric: correctness
severity: should_fix
---

**Rule.** When checking feature eligibility or gating behavior behind a runtime
feature flag, validate safe, stable non-flag preconditions (such as established
record types, user preferences, or device capabilities) before calling
`base::FeatureList::IsEnabled(...)` to avoid prematurely activating field trials
and enrolling ineligible clients into experiment study groups. Conversely,
evaluate `base::FeatureList::IsEnabled(...)` before executing newly introduced,
unverified, or potentially crashing code (such as new entity handlers, complex
parsing, or pointer dereferences) so the flag functions as an effective remote
kill switch.

**Why.** Evaluating `base::FeatureList::IsEnabled(...)` activates field trials
and registers clients into Finch study groups; checking it before verifying safe
baseline preconditions pollutes experiment metrics with ineligible users.
Conversely, any code executed before a feature check runs unconditionally for
all users even when disabled; executing unverified or crash-prone code before
the flag check prevents Finch kill switches from mitigating field outages and
forces emergency binary respins.

## Examples

Bad:

```cpp
// Anti-pattern 1: Evaluating flag before safe preconditions activates study for ineligible clients.
bool IsWalletReminderNoticeEligible(const CreditCard& card) {
  if (!base::FeatureList::IsEnabled(features::kAutofillEnableWalletReminderNotice)) {
    return false;
  }
  return card.record_type() == CreditCard::RecordType::kMaskedServerCard;
}

// Anti-pattern 2: Executing unverified or risky logic before flag check prevents kill-switch protection.
bool IsWalletReminderNoticeEligible(const EntityInstance& entity) {
  if (GetWalletPassType(entity.type(), entity.record_type()) !=
      EntityInstance::WalletPassType::kPublic) {
    return false;
  }
  return base::FeatureList::IsEnabled(features::kAutofillEnableWalletReminderNoticePublicPass);
}
```

Good:

```cpp
// Pattern 1: Validate safe preconditions before flag to prevent premature enrollment.
bool IsWalletReminderNoticeEligible(const CreditCard& card) {
  if (card.record_type() != CreditCard::RecordType::kMaskedServerCard) {
    return false;
  }
  return base::FeatureList::IsEnabled(features::kAutofillEnableWalletReminderNotice);
}

// Pattern 2: Guard new or unverified logic behind flag so the kill switch protects against crashes.
bool IsWalletReminderNoticeEligible(const EntityInstance& entity) {
  if (!base::FeatureList::IsEnabled(features::kAutofillEnableWalletReminderNoticePublicPass)) {
    return false;
  }
  return GetWalletPassType(entity.type(), entity.record_type()) ==
         EntityInstance::WalletPassType::kPublic;
}
```
