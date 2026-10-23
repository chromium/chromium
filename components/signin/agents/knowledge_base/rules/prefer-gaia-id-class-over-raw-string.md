---
name: signin/prefer-gaia-id-class-over-raw-string
rubric: design
severity: should_fix
---

**Rule.** Do not use raw strings (`std::string`, `std::string_view`, Java
`String`, or Objective-C `NSString*`) for storing or passing around Gaia IDs;
prefer the `GaiaId` class (`google_apis/gaia/gaia_id.h` in C++/Objective-C++ and
`org.chromium.google_apis.gaia.GaiaId` in Java) instead. Convert to or from raw
strings (`GaiaId::ToString()`, `GaiaId::ToNSString()`) only at serialization or
external API boundaries (such as prefs, protobufs, or network requests).

**Why.** Storing Gaia IDs in raw strings makes it easy to accidentally confuse a
Gaia ID with other string-based identifiers (such as email addresses,
`CoreAccountId`, or tokens). `GaiaId` provides compile-time type safety across
C++, Objective-C++, and Java (including automatic JNI conversion via
`@JniType("GaiaId")`).

## Examples

Bad:

```cpp
class AccountDataCache {
 public:
  void SetActiveGaiaId(const CoreAccountInfo& account) {
    active_gaia_id_ = account.gaia.ToString();
  }

  bool IsActiveAccount(const std::string& gaia_id) const {
    return active_gaia_id_ == gaia_id;
  }

 private:
  std::string active_gaia_id_;
};
```

Good:

```cpp
class AccountDataCache {
 public:
  void SetActiveGaiaId(const CoreAccountInfo& account) {
    active_gaia_id_ = account.gaia;
  }

  bool IsActiveAccount(const GaiaId& gaia_id) const {
    return active_gaia_id_ == gaia_id;
  }

 private:
  GaiaId active_gaia_id_;
};
```
