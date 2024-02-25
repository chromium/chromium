// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ITEM_LIST_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ITEM_LIST_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/payments/payment_request_row_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageView;
class View;
}

namespace payments {

class PaymentRequestDialogView;
class PaymentRequestSpec;
class PaymentRequestState;

// A control representing a list of selectable items in the PaymentRequest
// dialog. These lists enforce that only one of their elements be selectable at
// a time and that "incomplete" items (for example, a credit card with no known
// expiration date) behave differently when selected. Most of the time, this
// behavior is to show an editor screen.
class PaymentRequestItemList {
 public:
  // Represents an item in the item list.
  class Item : public PaymentRequestRowView {
    METADATA_HEADER(Item, PaymentRequestRowView)

   public:
    // Creates an item that will be owned by `list` with the initial state set
    // to `selected`. `clickable` indicates whether or not the user can interact
    // with this row. The `spec` parameter should not be null.
    Item(base::WeakPtr<PaymentRequestSpec> spec,
         base::WeakPtr<PaymentRequestState> state,
         PaymentRequestItemList* list,
         bool selected,
         bool clickable,
         bool show_edit_button);

    Item(const Item&) = delete;
    Item& operator=(const Item&) = delete;

    ~Item() override;

    bool selected() const { return selected_; }
    // Changes the selected state of this item to |selected|.
    // SelectedStateChanged is called if |notify| is true.
    void SetSelected(bool selected, bool notify);

    // Returns a pointer to the PaymentRequestItemList that owns this object.
    PaymentRequestItemList* list() { return list_; }

    // Returns a pointer to the PaymentRequestSpec/State objects associated with
    // this instance of the UI.
    base::WeakPtr<PaymentRequestSpec> spec() { return spec_; }
    base::WeakPtr<PaymentRequestState> state() { return state_; }

    // PaymentRequestRowView overrides
    // Leaf classes must override this and provide their own factory.
    base::WeakPtr<PaymentRequestRowView> AsWeakPtr() override = 0;

   protected:
    // Initializes the layout and content of the row. Must be called by subclass
    // constructors, so that virtual methods providing row contents are
    // accessible.
    void Init();

    // Called when the selected state of this item changes. Subclasses may
    // assume that they are the only selected item in |list| when this is
    // called. This could be called before CreateItemView so subclasses should
    // be aware that their views might not exist yet.
    virtual void SelectedStateChanged() = 0;

    // Creates an image of a large checkmark, used to indicate that an option is
    // selected.
    std::unique_ptr<views::ImageView> CreateCheckmark(bool selected);

    // Creates the view that represents this item's content. Typically this will
    // be a label describing the payment method, shipping adress, etc. Populates
    // |accessible_content| with the screen reader string for the returned
    // content. |accessible_content| shouldn't be null.
    virtual std::unique_ptr<views::View> CreateContentView(
        std::u16string* accessible_content) = 0;

    // Creates the view that should be displayed after the checkmark in the
    // item's view, such as the credit card icon.
    virtual std::unique_ptr<views::View> CreateExtraView();

    // Returns a string describing the type of data for which this row
    // represents an instance. e.g., "credit card" or "billing address". Used
    // when describing the row for accessibility.
    virtual std::u16string GetNameForDataType() = 0;

    // Returns whether this item is complete/valid and can be selected by the
    // user. If this returns false when the user attempts to select this item,
    // PerformSelectionFallback will be called instead.
    virtual bool CanBeSelected() = 0;

    // Performs the action that replaces selection when CanBeSelected returns
    // false. This will usually be to display an editor.
    virtual void PerformSelectionFallback() = 0;

    // Called when the edit button is pressed. Subclasses should open the editor
    // appropriate for the item they represent.
    virtual void EditButtonPressed() = 0;

   private:
    // Updates the accessible description of this item to reflect its current
    // status (selected/not).
    void UpdateAccessibleName();

    void ButtonPressed();

    base::WeakPtr<PaymentRequestSpec> spec_;
    base::WeakPtr<PaymentRequestState> state_;
    raw_ptr<PaymentRequestItemList, DanglingUntriaged> list_;
    std::u16string accessible_item_description_;
    bool selected_;
    bool show_edit_button_;
  };

  explicit PaymentRequestItemList(
      base::WeakPtr<PaymentRequestDialogView> dialog);

  PaymentRequestItemList(const PaymentRequestItemList&) = delete;
  PaymentRequestItemList& operator=(const PaymentRequestItemList&) = delete;

  virtual ~PaymentRequestItemList();

  // Adds an item to this list. |item->list()| should return this object.
  void AddItem(std::unique_ptr<Item> item);

  // Removes all items which have been added.
  void Clear();

  // Creates and returns the UI representation of this list. It iterates over
  // the items it contains, creates their associated views, and adds them to the
  // hierarchy.
  std::unique_ptr<views::View> CreateListView();

  // Deselects the currently selected item and selects |item| instead.
  void SelectItem(Item* item);

  base::WeakPtr<PaymentRequestDialogView> dialog() { return dialog_; }

 private:
  // Unselects the currently selected item. This is private so that the list can
  // use it when selecting a new item while avoiding consumers of this class
  // putting the list in a state where no item is selected.
  void UnselectSelectedItem();

  std::vector<std::unique_ptr<Item>> items_;
  raw_ptr<Item, DanglingUntriaged> selected_item_;
  base::WeakPtr<PaymentRequestDialogView> dialog_;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ITEM_LIST_H_
