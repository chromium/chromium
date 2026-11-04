---
name: chrome/encapsulate-content-analysis-audit-reporting-in-delegates
rubric: design
severity: should_fix
---

**Rule.** Encapsulate scan reporting, audit tracking, and user notification UI
(such as dialogs and copy/paste toasts) within ContentAnalysisDelegate and its
request handlers rather than exposing audit or reporting state in
ContentAnalysisDelegate::Result for callers to process. Restrict Result to the
authorization verdict (such as allow/block status) determining whether the
caller's action is permitted. If an action triggers an audit, warning, or
notification, handle UI presentation and telemetry internally within the
delegate (such as in ShowFinalResultInDialog() or ShowFinalResultInToast()).

**Why.** Callers invoking content analysis scans only need the decision verdict
to proceed with or abort the user action. Exposing audit flags or reporting
metadata in Result breaks encapsulation by leaking scan lifecycle details to
callers and forces caller integrations to duplicate notification and reporting
logic that belongs to the analysis delegate.

## Examples

Bad:

```cpp
// In ContentAnalysisDelegate::Result:
struct Result {
  // ...
  bool is_audit = false;
};

// In caller callback (e.g., data_protection_clipboard_utils.cc):
void OnScanComplete(const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
  if (result.is_audit) {
    enterprise_data_protection::MaybeShowCopyToast(
        profile, web_contents.get(),
        enterprise_data_protection::CopyToastType::kAudit);
  }
}
```

Good:

```cpp
// In ContentAnalysisDelegate::ShowFinalResultInToast():
bool ContentAnalysisDelegate::ShowFinalResultInToast() {
  // ...
  switch (final_result_) {
    case FinalContentAnalysisResult::SUCCESS:
      if (web_contents_) {
        enterprise_data_protection::MaybeShowCopyToast(
            profile_, web_contents_.get(),
            enterprise_data_protection::CopyToastType::kAudit);
      }
      return false;
    // ...
  }
}
```
