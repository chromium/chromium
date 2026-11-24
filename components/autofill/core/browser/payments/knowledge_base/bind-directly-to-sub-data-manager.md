---
name: components/autofill/core/browser/payments/bind-directly-to-sub-data-manager
description: In Autofill code accessing only payments or address data, bind directly to PaymentsDataManager or AddressDataManager instead of repeatedly accessing them through PersonalDataManager
rubric: readability
severity: nit
---

**Rule.** When a function or scope in Autofill only requires payments or address
data, bind a reference directly to `PaymentsDataManager` (via
`GetPersonalDataManager().payments_data_manager()`) or `AddressDataManager` (via
`GetPersonalDataManager().address_data_manager()`). Do not bind
`PersonalDataManager` locally when only its child manager is used, and avoid
repeatedly calling `.payments_data_manager()` or `.address_data_manager()`
across multiple call sites within the same scope.

**Why.** Binding directly to the specific sub-manager clarifies data
dependencies, reduces visual clutter, and eliminates redundant method chain
indirection on every access. It also avoids holding broader access to unrelated
address or payments data when only one domain is needed.

## Examples

Bad:

```cpp
// Bad: Binding the parent PersonalDataManager when only payments data is needed
// causes repeated indirection on every call.
const PersonalDataManager& personal_data_manager =
    autofill_client.GetPersonalDataManager();
auto cards =
    personal_data_manager.payments_data_manager().GetServerCreditCards();
for (const CreditCard* card : cards) {
  std::optional<CreditCardBenefit> benefit =
      personal_data_manager.payments_data_manager()
          .GetApplicableBenefitForCardAndOrigin(*card, origin, decider);
}
```

Good:

```cpp
// Good: Bind directly to PaymentsDataManager to clarify domain dependencies
// and simplify call sites.
const PaymentsDataManager& payments_data_manager =
    autofill_client.GetPersonalDataManager().payments_data_manager();
auto cards = payments_data_manager.GetServerCreditCards();
for (const CreditCard* card : cards) {
  std::optional<CreditCardBenefit> benefit =
      payments_data_manager.GetApplicableBenefitForCardAndOrigin(
          *card, origin, decider);
}
```
