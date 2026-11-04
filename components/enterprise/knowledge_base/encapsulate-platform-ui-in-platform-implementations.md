---
name: chrome/encapsulate-platform-ui-in-platform-implementations
rubric: design
severity: should_fix
---

**Rule.** Encapsulate platform-specific UI logic and divergent widget
presentations (such as an Android Toast vs. a desktop dialog) inside concrete
platform implementations of a UI factory or delegate interface. Do not branch
with compile-time #if BUILDFLAG(...) guards inside cross-platform business logic
or utility helpers to invoke platform-specific UI widgets directly.

**Why.** Inlining platform guards like #if BUILDFLAG(IS_ANDROID) in
cross-platform utilities introduces preprocessor clutter, bypasses platform UI
abstractions, and couples platform-agnostic business logic with native window or
widget lifecycles. Routing UI requests through an abstract factory or delegate
interface keeps cross-platform code clean and encapsulates platform-specific UI
presentation within the appropriate platform layer.

## Examples

Bad:

```cpp
// In cross-platform clipboard utils:
#if BUILDFLAG(IS_ANDROID)
ui::WindowAndroid* window = source.web_contents()->GetTopLevelNativeWindow();
window->ShowToast(
    l10n_util::GetStringUTF8(IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION));
#else
if (factory) {
  factory->ShowDialogIfNeeded(
      destination.web_contents(),
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
}
#endif
```

Good:

```cpp
// In cross-platform clipboard utils:
if (factory) {
  factory->ShowDialogIfNeeded(
      destination.web_contents(),
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
}

// In AndroidDataControlsDialogFactory::ShowDialogIfNeeded:
void AndroidDataControlsDialogFactory::ShowDialogIfNeeded(
    content::WebContents* web_contents,
    DataControlsDialog::Type type,
    base::OnceCallback<void(bool bypassed)> callback) {
  if (type == DataControlsDialog::Type::kClipboardPasteBlock ||
      type == DataControlsDialog::Type::kClipboardCopyBlock) {
    if (auto* window = web_contents->GetTopLevelNativeWindow()) {
      window->ShowToast(
          l10n_util::GetStringUTF8(IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION));
    }
    if (callback) {
      std::move(callback).Run(/*bypassed=*/false);
    }
    return;
  }
  DataControlsDialogFactory::ShowDialogIfNeeded(web_contents, type,
                                                std::move(callback));
}
```
