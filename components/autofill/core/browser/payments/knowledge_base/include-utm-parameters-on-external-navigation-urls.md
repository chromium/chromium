---
name: components/autofill/core/browser/payments/include-utm-parameters-on-external-navigation-urls
description: When adding or updating external service navigation URLs launched from Chrome UI or settings, verify outbound campaign telemetry parameters like utm_campaign are included consistently
rubric: testing
severity: should_fix
---

**Rule.** When adding or updating navigation URLs pointing to external services
(such as Google Wallet, Account, or Pay) launched from Chrome UI, settings, or
dialogs, verify that outbound campaign telemetry query parameters (specifically
`utm_campaign`, `utm_source`, and `utm_medium`) are confirmed with stakeholders
and included consistently with sibling endpoints. Do not omit `utm_campaign` or
feature attribution parameters when linking to external web consoles.

**Why.** External services rely on standard UTM query parameters to measure
inbound referral traffic, attribute engagement to specific Chrome entry points,
and analyze user conversion funnels. Omitting `utm_campaign` or inconsistent
attribution parameters breaks partner and service-side analytics.

## Examples

Bad:

```cpp
const char kProdWalletManageSettingsUrl[] =
    "https://wallet.google.com/wallet?"
    "p=settings&utm_source=chrome&utm_medium=settings";
```

Good:

```cpp
const char kProdWalletManageSettingsUrl[] =
    "https://wallet.google.com/wallet?"
    "p=settings&utm_source=chrome&utm_medium=settings&utm_campaign=settings";
```
