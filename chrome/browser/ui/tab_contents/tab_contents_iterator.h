// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_ITERATOR_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_ITERATOR_H_

#include <iterator>
#include <optional>

#include "base/memory/stack_allocated.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace content {
class WebContents;
}

// Iterates through all tab contents in all browser windows. Because the
// renderers act asynchronously, getting a tab contents through this interface
// does not guarantee that the renderer is ready to go. Doing anything to affect
// browser windows or tabs while iterating may cause incorrect behavior.
//
// Examples:
//
//   for (auto* web_contents : AllTabContentses()) {
//     SomeFunctionTakingWebContents(web_contents);
//     -or-
//     web_contents->OperationOnWebContents();
//     ...
//   }
//
//   auto& all_tabs = AllTabContentses();
//   auto it = some_std_algorithm(all_tabs.begin(), all_tabs.end(), ...);

class AllTabContentsesList {
 public:
  class Iterator {
    STACK_ALLOCATED();

   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = content::WebContents*;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = const value_type&;

    Iterator();
    Iterator(const Iterator& iterator);
    ~Iterator();

    value_type operator->() const { return cur_; }
    reference operator*() const { return cur_; }

    Iterator& operator++() {
      Next();
      return *this;
    }

    Iterator operator++(int) {
      Iterator it(*this);
      Next();
      return it;
    }

    bool operator==(const Iterator& other) const { return cur_ == other.cur_; }

    // Returns the Browser instance associated with the current tab contents.
    // Valid as long as this iterator != the AllTabContentses().end() iterator.
    Browser* browser() const {
      return browser_iterator_ == BrowserList::GetInstance()->end()
                 ? nullptr
                 : *browser_iterator_;
    }

   private:
    friend class AllTabContentsesList;
    explicit Iterator(bool is_end_iter);

    // Loads the next tab contents into |cur_|. This is designed so that for the
    // initial call from the constructor, when |browser_iterator_| points to the
    // first Browser and |tab_index_| is -1, it will fill the first tab
    // contents.
    void Next();

    // Current WebContents, or null if we're at the end of the list. This can be
    // extracted given the browser iterator and index, but it's nice to cache
    // this since the caller may access the current tab contents many times.
    content::WebContents* cur_;

    // An iterator over all the browsers.
    BrowserList::const_iterator browser_iterator_;

    // An iterator for tabs in a tabstrip.
    std::optional<TabStripModel::TabIterator> tab_iterator_;
  };

  using iterator = Iterator;
  using const_iterator = Iterator;

  const_iterator begin() const { return Iterator(false); }
  const_iterator end() const { return Iterator(true); }

  AllTabContentsesList() = default;
  ~AllTabContentsesList() = default;
};

// TODO(crbug.com/431671320): Replace all current usage of this with
// `ForEachTabInterface()` and remove this.
const AllTabContentsesList& AllTabContentses();

namespace tabs {

// Iterates through all TabInterfaces across all Profiles and Browsers. If
// `on_tab` returns false iteration is stopped.
void ForEachTabInterface(base::FunctionRef<bool(tabs::TabInterface*)> on_tab);

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_ITERATOR_H_
