---
name: autofill/document-lifetime-and-owner-of-manager-and-client-classes
rubric: readability
severity: should_fix
---

**Rule.** Every new class whose name ends in `Manager`, `Client`, `Driver`,
`Delegate` or `Controller` carries a class comment stating its cardinality and
owner, for example "One instance per tab, owned by AutofillClient." A comment
that only describes what the class does is not sufficient.

**Why.** Autofill composes dozens of such classes across profile, tab, frame and
form scopes. Without a one-line ownership statement, readers must trace
factories and client implementations to learn when an object exists, which is
the most common source of dangling-pointer and lifetime bugs in this component.

## Examples

Bad:

```cpp
// Coordinates IBAN suggestions and filling across frames.
class IbanManager {
 public:
  explicit IbanManager(AutofillClient* client);
  // ...
};
```

Good:

```cpp
// Coordinates IBAN suggestions and filling across frames.
// One instance per tab, owned by AutofillClient.
class IbanManager {
 public:
  explicit IbanManager(AutofillClient* client);
  // ...
};
```
