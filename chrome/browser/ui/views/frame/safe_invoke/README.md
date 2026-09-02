# SafeInvoke & SafeChain

`SafeInvoke` provides safe navigation/optional chaining in complex C++
expressions. It is similar to the `?.` operator in languages like TypeScript.

If the underlying pointer is `nullptr` (or if a callable is null/unbound), all
subsequent `.Then(...)` calls in the chain are safely skipped without crash or
undefined behavior.

---

## Overview

Using `SafeInvoke` turns nested calls like this:

```cpp
if (browser_view_) {
  if (auto* browser_widget = browser_view_->browser_widget()) {
    if (auto* frame_view = browser_widget->GetFrameView()) {
      frame_view->SchedulePaint();
    }
  }
}
```

into this:

```cpp
SafeInvoke(browser_view_)
    .Then(&BrowserView::browser_widget)
    .Then(&BrowserWidget::GetFrameView)
    .Then(&views::View::SchedulePaint);
```

If any step in the chain fails to produce a valid value, the expression safely
short-circuits and yields a null result matching the final return type:

```cpp
// Value methods return std::optional<T>, defaulted via .value_or():
const int panel_width = SafeInvoke(side_panel_coordinator_)
                          .Then(&SidePanelCoordinator::GetHeaderView)
                          .Then(&views::View::width)
                          .value_or(0);

// Pointer methods return nullptr if any step in the chain is null:
views::ToggleImageButton* pin_button =
    SafeInvoke(side_panel_coordinator_)
        .Then(&SidePanelCoordinator::GetHeaderView)
        .Then(&SidePanelHeader::header_pin_button)
        .get();
```

---

## Usage

### Warnings

- **Do not store intermediate chain results:** `SafeChain` is an ephemeral
  stack temporary designed exclusively for single-expression chaining. Do not
  store the result of calling `.Then()` across statements, as it holds
  non-owning raw pointers.
- **Eager argument evaluation:** Arguments passed to `.Then(fn, args...)` are
  evaluated eagerly at the call site before `.Then()` executes. If an
  argument expression has side effects, those side effects will execute even
  if the target object is null and the method is skipped. To evaluate
  arguments lazily, pass a callback or closure.
- **Unbound callbacks & null function pointers:** `SafeInvoke` guards
  against null object pointers in the navigation chain, not null callables.
  If the target object is non-null, attempting to invoke an unbound callback
  or null function pointer will trigger a `DCHECK`/crash as a programming
  error.
  - TODO(crbug.com/555733301): Unbound callbacks currently trigger a
    `DCHECK`/crash on non-null targets. We may introduce a specialized
    function (such as `.ThenIfCallIsValid()`) to allow optional callback
    execution in the future.

---

### Supported Callables

- **Member function pointers:** `&Class::Method` - method is called on the
  result of the previous step.
- **Unary / free functions:** `&FreeFunction` - result of the previous step is
  passed as the first argument.
- **Lambdas & Functors:** `[](Type* item) { ... }` or `[](auto*) { ... }`
- **Chromium Callbacks:** `base::RepeatingCallback` / `base::OnceCallback` -
  invoked with the result of the previous step.

---

### Return Value Types

If the final `.Then()` call has a function that returns:
- **`void`**: return value is `void`, side-effects happen if the chain
  succeeded to that point.
- **Value type `T`**: return value is `std::optional<T>`.
- **Pointer or reference (`T*` / `T&`)**: an internal object;
  - you can call `.Then()` to keep chaining, or
  - you can call `.get()` to retrieve a raw pointer, which will be `nullptr` if
    any step in the chain failed.

---

## Examples

### 1. Void Calls & Side Effects
```cpp
// Single or multi-step execution with arguments:
SafeInvoke(side_panel)
    .Then(&SidePanel::DisableAnimationsForTesting);

SafeInvoke(header)
    .Then(&SidePanelHeader::SetTitle, new_title);
```

### 2. Navigating Object Hierarchies
```cpp
// Safely navigate nested views to retrieve a pointer:
content::WebContents* contents =
    SafeInvoke(browser_view_)
        .Then(&BrowserView::GetActiveTabInterface)
        .Then(&tabs::TabInterface::GetContents)
        .get();
```

### 3. Value Extraction
```cpp
// Value-returning methods return std::optional:
std::optional<bool> is_showing =
    SafeInvoke(entry).Then(&SidePanelEntry::IsShowing);

if (is_showing.value_or(false)) {
  // ...
}
```

### 4. Chromium Callbacks
```cpp
// Pass callbacks that accept the navigated pointer:
SafeInvoke(entry).Then(base::BindRepeating(&Entry::Close));
SafeInvoke(entry).Then(std::move(once_callback));
```

### 5. Overload Disambiguation
```cpp
// Disambiguate overloaded non-const or const member functions:
views::View* subview =
    SafeInvoke(parent_view)
        .Then(Overload<ui::ElementIdentifier>(
                  &views::View::GetViewByElementId),
              kSidePanelPinButtonElementId)
        .get();

const views::View* const_subview =
    SafeInvoke(const_parent_view)
        .Then(ConstOverload<ui::ElementIdentifier>(
                  &views::View::GetViewByElementId),
              kSidePanelPinButtonElementId)
        .get();
```

### 6. Contextual Boolean Conversion
```cpp
// Check if an object exists:
if (SafeInvoke(view)) {
  // view is non-null
}

if (SafeInvoke(view).Then(&views::View::GetWidget)) {
  // widget exists and is non-null
}
```
