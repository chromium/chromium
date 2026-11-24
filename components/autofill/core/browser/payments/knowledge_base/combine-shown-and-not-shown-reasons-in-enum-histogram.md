---
name: components/autofill/core/browser/payments/combine-shown-and-not-shown-reasons-in-enum-histogram
description: When logging telemetry for UI presentation or feature display decisions, record a unified enum histogram with a shown bucket rather than separate histograms
rubric: testing
severity: should_fix
---

**Rule.** When instrumenting telemetry for UI presentation, prompt display, or
feature triggering attempts, record a unified enumeration histogram (e.g.,
<Feature>.ShowResult) where one baseline bucket represents successful display
(such as kShown = 0) and other buckets represent each reason not shown. Do not
record a separate not-shown reason enum alongside a distinct shown counter or
boolean histogram.

**Why.** Combining presentation success and suppression reasons into a single
enum histogram ensures that the total bucket count represents the true
denominator of all presentation attempts. This provides a direct, interpretable
ratio of shown vs. not-shown events and avoids discrepancies caused by backend
data filtering or client-level exclusions that can affect separate histograms
differently.

## Examples

Bad:

```cpp
// Separate boolean for shown and enum for not-shown reasons:
void LogPaymentsBubbleShown(bool is_reshow);
void LogPaymentsBubbleNotShownReason(PaymentsBubbleNotShownReason reason) {
  base::UmaHistogramEnumeration(
      "Autofill.PaymentsBubble.NotShownReason", reason);
}
```

Good:

```cpp
// Unified enum where bucket 0 is "Shown" and subsequent buckets are reasons not shown:
enum class PaymentsBubbleShowResult {
  kShown = 0,
  kOffTheRecord = 1,
  kNoCachedForm = 2,
  kFeatureDisabled = 3,
  kMaxValue = kFeatureDisabled,
};

void LogPaymentsBubbleShowResult(PaymentsBubbleShowResult result) {
  base::UmaHistogramEnumeration(
      "Autofill.PaymentsBubble.ShowResult", result);
}
```
