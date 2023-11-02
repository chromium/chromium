## Adding metrics for a new feature?

Please do **not** add your metrics in `autofill_metrics.*`! Instead, help
support code clarity and organization by adding a pair of files **per feature**.

### The old way

Originally, when making metrics for a new feature, everything was thrown into
the `autofill_metrics.*` files:

*autofill_metrics.h*:

```c++ {.bad}
class AutofillMetrics {

  enum class CoolFeatureInteractionMetric {
    // User accepted the cool feature.
    kAccepted = 0,
    // User explicitly denied the feature.
    kCancelled = 1,
    // User left the page without interacting with the feature.
    kIgnored = 2,
    kMaxValue = kIgnored,
  };

  // ...Roughly 1500 lines of other enums and functions...

  static void LogCoolFeatureInteraction(CoolFeatureInteractionMetric metric);
}
```

*autofill_metrics.cc*:

```c++ {.bad}
  // ...In the middle of 3000 lines of similar code...

  // static
  void AutofillMetrics::LogCoolFeatureInteraction(
      CoolFeatureInteractionMetric metric) {
    base::UmaHistogramEnumeration("Autofill.CoolFeatureInteraction", metric);
  }
```

The calling code of this function would be
`AutofillMetrics::LogCoolFeatureInteraction(~)`.

The biggest downside to this approach was not just how large the two files
became, but also how far apart enums/metrics related to the same feature were
located, as well as how hard it was to find **all** metrics for a given feature.

### The new way

**Don't** use a class, but **do** use the `autofill::autofill_metrics` namespace. Then,
combine all metrics that are part of a single feature together, so they're not
intertwined with the rest of Autofill's metrics.

*cool_feature_metrics.h*:

(Put this header file under components/autofill/core/browser/metrics/[optional_sub_directory])
```c++ {.good}
// [Copyright notice and include guards]

namespace autofill::autofill_metrics {

enum class CoolFeatureInteractionMetric {
  // User accepted the cool feature.
  kAccepted = 0,
  // User explicitly denied the feature.
  kCancelled = 1,
  // User left the page without interacting with the feature.
  kIgnored = 2,
  kMaxValue = kIgnored,
};

void LogCoolFeatureInteraction(CoolFeatureInteractionMetric metric);

}  // namespace autofill::autofill_metrics
```

*cool_feature_metrics.cc*:

```c++ {.good}
// [Copyright notice]

#include "components/autofill/core/browser/metrics/[optional_sub_directory]/cool_feature_metrics.h"

namespace autofill::autofill_metrics {

void LogCoolFeatureInteraction(CoolFeatureInteractionMetric metric) {
  base::UmaHistogramEnumeration("Autofill.CoolFeatureInteraction", metric);
}

// If there are other metrics related to this feature, they'd go here, and it
// would be very obvious they're related!

}  // namespace autofill::autofill_metrics
```

The calling code of this function would be
`autofill::autofill_metrics::LogCoolFeatureInteraction(~)`.
