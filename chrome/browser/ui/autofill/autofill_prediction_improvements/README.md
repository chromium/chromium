This directory contains files that use Autofill UI infrastructure but are
related to Autofill Prediction Improvements.

They are in an Autofill subdirectory (as opposed to, e.g.,
`//chrome/browser/ui/autofill_prediction_improvements`) because of their
dependencies:
- They depend on Autofill UI code by extending from `AutofillBubbleBase` and
  `AutofillBubbleControllerBase`.
- Autofill UI code depends on them because the view is instantiated
  via `AutofillBubbleHandler`.
