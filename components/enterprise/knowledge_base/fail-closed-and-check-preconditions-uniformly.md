---
name: enterprise/fail-closed-and-check-preconditions-uniformly
rubric: security
severity: blocking
---

**Rule.** Enforce pointer and context preconditions (such as `CHECK(prefs)` or
`CHECK(profile)`) unconditionally at function entry before any
`#if BUILDFLAG(...)` branches, and when gating dev or staging overrides, default
release-channel parameters to the most restrictive fail-closed value (e.g.,
`version_info::Channel::STABLE` rather than `UNKNOWN`, using an exhaustive
`switch (channel)` with no `default:` branch).

**Why.** Placing `CHECK`s inside `#if BUILDFLAG(...)` blocks allows null pointer
dereferences or broken caller invariants to go undetected on platforms where the
branch is compiled out. When gating dev or staging overrides, defaulting channel
checks to `UNKNOWN` or using a permissive `default:` case risks accidentally
enabling dev/staging bypasses in production if a new channel enum value is added
or omitted by a caller.

## Examples

Bad:

```cpp
bool IsDevOverrideAllowed(PrefService* prefs,
                          version_info::Channel channel =
                              version_info::Channel::UNKNOWN) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  CHECK(prefs);
#endif
  switch (channel) {
    case version_info::Channel::STABLE:
    case version_info::Channel::BETA:
      return false;
    default:
      return true;
  }
}
```

Good:

```cpp
bool IsDevOverrideAllowed(PrefService* prefs,
                          version_info::Channel channel =
                              version_info::Channel::STABLE) {
  CHECK(prefs);
  switch (channel) {
    case version_info::Channel::STABLE:
    case version_info::Channel::BETA:
    case version_info::Channel::UNKNOWN:
      return false;
    case version_info::Channel::DEV:
    case version_info::Channel::CANARY:
      return prefs->GetBoolean(prefs::kAllowDevEndpoints);
  }
  NOTREACHED();
}
```
