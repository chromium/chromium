---
name: autofill/own-tab-scoped-managers-in-autofill-client
rubric: design
severity: should_fix
---

**Rule.** If a new Autofill manager (for example `IbanManager`) tracks per-tab
interactions or records tab-specific metrics, create and own it inside
`AutofillClient` or `PaymentsAutofillClient` (one instance per tab). Avoid lazy
initialization; instantiate the manager eagerly when the client is constructed
unless deferred creation is explicitly justified. Do not model it as a
`KeyedService` with a `ProfileKeyedServiceFactory` unless the state is genuinely
profile-wide and the change explains why.

**Why.** A profile-keyed service is shared by every tab of the profile, so two
tabs interacting with forms at the same time overwrite each other's state and
attribute metrics to the wrong interaction. Ownership in the client matches the
tab lifetime and keeps the object graph discoverable from `AutofillClient`, and
eager initialization makes the manager's lifecycle predictable.

## Examples

Bad:

```cpp
// Per-tab interaction state stored in a profile-wide KeyedService.
class IbanManager : public KeyedService {
 public:
  explicit IbanManager(Profile* profile);
  void OnDidFillIban(const FormGlobalId& form_id);

 private:
  FormGlobalId last_filled_form_;
};

class IbanManagerFactory : public ProfileKeyedServiceFactory {
  // One instance per Profile, shared by all tabs.
};
```

Good:

```cpp
// Tab-scoped manager owned and eagerly initialized by the tab's AutofillClient.
class ChromeAutofillClient : public ContentAutofillClient {
 public:
  IbanManager* GetIbanManager() override { return iban_manager_.get(); }

 private:
  std::unique_ptr<IbanManager> iban_manager_{
      std::make_unique<IbanManager>(this)};
};
```
