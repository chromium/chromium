---
name: enterprise/gate-new-enterprise-features
rubric: design
severity: should_fix
---

**Rule.** Default new enterprise `BASE_FEATURE` flags to
`base::FEATURE_DISABLED_BY_DEFAULT` prior to launch approval (unless the flag is
a defensive bugfix or migration killswitch, or strictly an emergency killswitch
for functionality already 100% gated behind an unset-by-default enterprise
policy), and use a single feature flag per cohesive capability rather than
separate flags per event type.

**Why.** Enabling unfinished enterprise features by default risks unapproved
behavior or telemetry in production builds, while proliferating per-event flags
creates combinatorial testing and rollout complexity. Defensive bugfix,
migration, or policy-gated killswitches may use
`base::FEATURE_ENABLED_BY_DEFAULT` when the intended default behavior is already
safe or policy-controlled.

## Examples

Bad:

```cpp
// Unlaunched new reporting feature enabled by default and split per event:
BASE_FEATURE(kEnterpriseSecurityLoginEventReporting,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnterpriseSecurityLogoutEventReporting,
             base::FEATURE_ENABLED_BY_DEFAULT);
```

Good:

```cpp
// Cohesive new feature disabled by default until launch approval:
BASE_FEATURE(kEnterpriseSecurityEventReporting,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Defensive bugfix killswitch enabled by default:
BASE_FEATURE(kBypassURLBlocklistWildcardForInternalChromeUrls,
             base::FEATURE_ENABLED_BY_DEFAULT);
```
