---
name: signin/do-not-branch-behavior-on-signin-access-point
rubric: design
severity: should_fix
---

**Rule.** Do not use `signin_metrics::AccessPoint` or `@SigninAccessPoint` to
change user-visible behavior; pass explicit configuration parameters or
delegates instead. Logging/metrics recording is the only exception where
inspecting `AccessPoint` is allowed, provided there is only a small, fixed set
of entry points requiring metrics special-casing (avoid adding `switch`
statements that list all values in `signin_metrics::AccessPoint`).

**Why.** `signin_metrics::AccessPoint` is a metrics identifier with more than a
hundred entry points that evolve frequently. Branching product or UI behavior
on `AccessPoint` couples feature logic to telemetry enums, and exhaustive
`switch` statements over all `AccessPoint` values force every new entry point to
make arbitrary decisions in unrelated code.

## Examples

Bad:

```cpp
void OnSigninCompleted(signin_metrics::AccessPoint access_point) {
  // BAD: Changing user-visible UI behavior based on AccessPoint.
  if (access_point == signin_metrics::AccessPoint::kBookmarkBubble ||
      access_point == signin_metrics::AccessPoint::kPasswordBubble) {
    ShowConfirmationToast();
  }

  // BAD: Exhaustive switch over all AccessPoint values.
  switch (access_point) {
    case signin_metrics::AccessPoint::kStartPage:
    case signin_metrics::AccessPoint::kMenu:
    case signin_metrics::AccessPoint::kSettings:
    // ... all other AccessPoint values ...
      RecordCustomMetric(access_point);
      break;
  }
}
```

Good:

```cpp
void OnSigninCompleted(signin_metrics::AccessPoint access_point,
                       signin_metrics::PromoAction promo_action,
                       bool show_confirmation_toast) {
  if (show_confirmation_toast) {
    ShowConfirmationToast();
  }

  signin_metrics::LogSigninAccessPointCompleted(access_point, promo_action);
  if (access_point == signin_metrics::AccessPoint::kBookmarkBubble) {
    base::UmaHistogramBoolean("Signin.BookmarkBubble.Completed", true);
  }
}
```
