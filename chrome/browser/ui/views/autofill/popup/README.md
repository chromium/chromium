# Popups

The popup classes in this directory are used to display the following types of
data on Desktop platforms:
* Autofill profile data (i.e. addresses)
* Autocomplete data
* Payment data

Additionally, `PopupBaseView` is also re-used for the popup in which
password generation prompts are rendered.

## Class hierarchy

The main classes for the popup have the following hierarchy:
```
┌───────────────────────┐
│ PopupBaseView         │
└──▲────────────────────┘
   │ extends
┌──┴────────────────────┐
│ PopupViewViews        │
└──┬────────────────────┘
   │creates (via CreatePopupRowView()) and owns N
┌──▼────────────────────┐
│ PopupRowView          │
└──┬────────────────────┘
   │owns
┌──▼────────────────────┐
│ PopupRowContentView   │
└───────────────────────┘

┌───────────────────────┐
│ PopupRowView          │
└──▲────────────────────┘
   │ extends
┌──┴─────────────────────┐
│ PopupRowWithButtonView │
└────────────────────────┘
```

These classes serve the following purposes:
* [`PopupBaseView`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/autofill/popup/popup_base_view.h): Serves as the common base class for both `PopupViewViews` and `PasswordGenerationPopupViewViews`.
  * It is responsible for creating a widget and setting itself as its content area.
  * It checks that there is enough space for showing the popup and creates a border with an arrow pointing to
    the HTML form field from which the popup was triggered.
* [`PopupViewViews`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/autofill/popup/popup_view_views.h): Implements the `AutofillPopupView` interface that the `AutofillPopupController`
   interacts with.
   * It serves as a container for multiple popup rows (e.g. encapsulating the non-footer
   rows in a `ScrollView`). Rows with selectable cells are represented by `PopupRowView`, but `PopupViewViews` can
   also contain rows that cannot be selected (e.g. a `PopupSeparatorView`).
   * It is responsible for processing keyboard events that change the cell selection across rows (e.g. cursor down)
     for announcing selection changes to the accessibility system.
* [`PopupRowView`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/autofill/popup/popup_row_view.h): Represents a single row in a `PopupViewViews`.
   * It accepts a `PopupRowContentView` in its constructor and takes ownership of it. This content
     view occupies the majority of the row view area and basically determines its appearance.
   * It handles native mouse events and keyboard events (that pass through `RenderWidgetHost`) to control suggestion selection (form
     preview) and acceptance (form filling).
   * It automatically adds an expanding control for suggestions with children. Note that
     the sub-popup is not controlled by the row, but in `PopupViewViews`
* [`PopupRowWithButtonView`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/autofill/popup/popup_row_with_button_view.h): An extension of `PopupRowView` that supports
   a single button in the content view layout.
   * It supports the subtleties of having a control inside a row view, e.g. selection/unselection
     on button focusing.
   * An example of usage is the Autocomplete suggestion with a "Delete" button
* [`CreatePopupRowView()`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h): Row construction method.
   * It is the entry point for creating rows of different types. Based on the suggestion (mostly
     its `SuggestionType`) it instantiates a row class and its content view.


## Adding new popup content

There are currently about ~30 different `SuggestionType`s, that correspond to a different type of popup row. When a new type is added and it cannot be properly processed by `CreatePopupRowView()`, use the
following steps to support it by the popup UI:
* Create a new factory method for `PopupRowContentView` (or even for `PopupRowView` if needed, e.g. `CreateAutocompleteRowWithDeleteButton()`),
  e.g. `CreatePasswordPopupRowContentView()` in [popup_row_factory_utils.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.cc).
* Call it when appropriate in [`CreatePopupRowView()`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.cc).
* Add/update tests in `popup_row_factory_utils_unittest.cc` if some testable logic was affected.
* Add a pixel test to `popup_row_factory_utils_browsertest.cc`. In most cases adding the newly added suggestion type to `kSuggestions` will suffice.
