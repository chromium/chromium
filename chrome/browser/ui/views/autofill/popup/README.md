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
   │owns N
┌──▼────────────────────┐ owns 1 ┌────────────────────┐
│ PopupRowView          ├────────► PopupRowStrategy   │
└──┬────────────────────┘        └────┬───────────────┘
   │owns 1 or 2                       │
┌──▼────────────────────┐  creates    │
│ PopupCellView         ◄─────────────┘
└───────────────────────┘
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
* [`PopupRowView`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/autofill/popup/popup_row_view.h): Represents a single row in a `PopupViewViews` that has selectable cells.
   * It can have one or two cells (represented by a `PopupCellView`) and it renders them next to each other. The first
     cell is the "content" cell; the optional second cell is a "control" cell, e.g. for displaying a delete button.
   * It owns a `PopupRowStrategy` that it uses to create the contained cells at construction time.
   * It is responsible for processing keyboard events that affect only elements inside the row (e.g. cursor right).
* [`PopupCellView`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/autofill/popup/popup_cell_view.h): Represents the smallest selectable element in a popup, a cell.
   * It processes mouse events and triggers callbacks (e.g. on mouse enter)
   * By default, it is a container: `PopupRowStrategy` can add arbitrary content to it.
* [`PopupRowStrategy`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/autofill/popup/popup_row_strategy.h): Acts as an interface to create `PopupCellView`s for different types of popup entries. It is passed
   to `PopupRowView` at construction. The classes that extend it (e.g. a `PopupPasswordSuggestionStrategy`)
   add content to the cell view and set the proper callbacks to the cell view for processing selection and
   click events.


## Adding new popup content

There are currently about ~30 different `PopupItemId`s, that correspond to a different type of popup row. When a new type is added and it cannot be properly processed by an existing `PopupRowStrategy`, use the
following steps to support it by the popup UI:
* Create a new strategy that implements `PopupRowStrategy`:
  * To customize the displayed content of a cell, simply add child views to the `PopupCellView` that `CreateContent()` or `CreateControl()` create - it behaves as any other container view.
  * To customize the interaction behavior of a cell, use the callback methods that `PopupCellView` exposes. For
    example, you can call `PopupCellView::SetOnAcceptedCallback(base::RepeatingClosure)` to set the behavior
    for accepting a cell.
* Add a switch case to [`PopupRowView::Create`](https://source.chromium.org/search?q=PopupRowView::Create) to
  pass the new strategy when the `Suggestion` has the relevant `PopupItemId`.
* Add tests to `popup_row_strategy_unittest.cc` to test the accessibility and selection behavior of the cells and
  to `popup_view_views_browsertest.cc` to add pixel tests.
