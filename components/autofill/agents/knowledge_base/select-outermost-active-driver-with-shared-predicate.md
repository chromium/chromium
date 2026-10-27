---
name: autofill/select-outermost-active-driver-with-shared-predicate
rubric: correctness
severity: should_fix
---

**Rule.** Avoid interacting with `AutofillDriver` directly from feature code;
prefer going through `AutofillManager` instead. When code (such as
`AutofillClient`) genuinely needs to work with the drivers of a tab, iterate
`client.GetAutofillDriverFactory().GetExistingDrivers()` and filter the list
using `driver->IsActive()`, `driver->GetParent()` and `driver->IsEmbedded()`
according to which frames the use case concerns. Do not take the first driver,
check only `GetParent()`, or invent another heuristic.

**Why.** Pages contain prerendered, back-forward-cached, fenced and embedded
frames whose drivers also appear in the factory, and forms may live in child
frames. The three predicates let code state precisely which frames it means:
active vs. prerendered or back-forward-cached, root vs. child, outermost vs.
embedded by a `<fencedframe>` or GuestView. Partial checks silently pick a wrong
frame, and dropping all non-root drivers silently loses forms in child frames;
both produce suggestions, metrics or fills for the wrong document.

## Examples

Bad:

```cpp
// Only checks GetParent(); may select an inactive or embedded frame.
AutofillDriver* driver = nullptr;
for (AutofillDriver* d :
     client.GetAutofillDriverFactory().GetExistingDrivers()) {
  if (!d->GetParent()) {
    driver = d;
    break;
  }
}
```

Good:

```cpp
// Applies the full predicate: active, no parent, not embedded.
auto drivers = client.GetAutofillDriverFactory().GetExistingDrivers();
auto it = std::ranges::find_if(drivers, [](AutofillDriver* d) {
  return d->IsActive() && !d->GetParent() && !d->IsEmbedded();
});
AutofillDriver* driver = it != drivers.end() ? *it : nullptr;
```
