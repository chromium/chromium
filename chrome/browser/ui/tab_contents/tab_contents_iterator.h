// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_ITERATOR_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_ITERATOR_H_

#include <iterator>

#include "base/memory/stack_allocated.h"
#include "chrome/browser/ui/browser_list.h"

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

    value_type operator->() { return cur_; }
    reference operator*() { return cur_; }

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

    bool operator!=(const Iterator& other) const { return !(*this == other); }

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

    // Tab index into the current Browser of the current tab contents.
    int tab_index_;

    // Current WebContents, or null if we're at the end of the list. This can be
    // extracted given the browser iterator and index, but it's nice to cache
    // this since the caller may access the current tab contents many times.
    content::WebContents* cur_;

    // An iterator over all the browsers.
    BrowserList::const_iterator browser_iterator_;
  };

  using iterator = Iterator;
  using const_iterator = Iterator;

  const_iterator begin() const { return Iterator(false); }
  const_iterator end() const { return Iterator(true); }

  AllTabContentsesList() = default;
  ~AllTabContentsesList() = default;
};

const AllTabContentsesList& AllTabContentses();

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_ITERATOR_H_
