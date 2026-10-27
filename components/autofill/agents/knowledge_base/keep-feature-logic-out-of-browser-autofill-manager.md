---
name: autofill/keep-feature-logic-out-of-browser-autofill-manager
rubric: design
severity: should_fix
---

**Rule.** When implementing a feature, do not add feature-specific logic, state,
or metrics to `AutofillManager`, `BrowserAutofillManager`, or
`AutofillPopupControllerImpl`. Implement a dedicated class (for example
`FooManager` under `integrators/foo/`) that observes the manager through
`AutofillManager::Observer` or `ScopedAutofillManagersObservation` and reacts to
hooks such as `OnAfterAskForValuesToFill`. If `AutofillManager::Observer` does
not yet expose the needed lifecycle event, add a generic observer notification
there rather than branching on the feature inside the manager.

**Why.** `BrowserAutofillManager` and the popup controller sit on the hot path
of every form interaction and are already the largest classes in Autofill. Each
feature that moves in increases coupling, makes behaviour harder to reason about
and test, and forces unrelated features to share a lifetime. Observer-based
integrators keep feature code independently owned and removable.

## Examples

Bad:

```cpp
// Feature logic and metrics embedded in BrowserAutofillManager.
void BrowserAutofillManager::OnAskForValuesToFillImpl(
    const FormData& form,
    const FormFieldData& field) {
  if (base::FeatureList::IsEnabled(features::kAutofillPlusAddresses)) {
    MaybeShowPlusAddressSuggestions(form, field);
    base::UmaHistogramBoolean("Autofill.PlusAddress.Shown", true);
  }
}
```

Good:

```cpp
// Dedicated integrator observes the manager and owns the feature logic.
void PlusAddressManager::OnAfterAskForValuesToFill(
    AutofillManager& manager,
    const FormData& form,
    const FormFieldData& field) {
  MaybeShowPlusAddressSuggestions(form, field);
  base::UmaHistogramBoolean("Autofill.PlusAddress.Shown", true);
}
```
