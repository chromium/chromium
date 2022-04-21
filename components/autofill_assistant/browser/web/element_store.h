// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_STORE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_STORE_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/web/element.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill_assistant {
class ElementFinderResult;

// Temporary store for elements resolved from a |Selector| by the
// |ElementFinder|. This store only holds a shallow copy of the element,
// outgoing elements need to be reconstructed first.
class ElementStore {
 public:
  // |web_contents| must outlive this instance.
  ElementStore(content::WebContents* web_contents);
  virtual ~ElementStore();

  ElementStore(const ElementStore&) = delete;
  ElementStore& operator=(const ElementStore&) = delete;

  // Add a new element to the store. This overwrites any previously existing
  // element with the same id.
  void AddElement(const std::string& client_id,
                  const DomObjectFrameStack& object);

  // Get an element from the store. If the element does not exist or cannot be
  // reconstructed this returns an error status.
  virtual ClientStatus GetElement(const std::string& client_id,
                                  ElementFinderResult* out_element) const;

  // Restore an element. If the element cannot be reconstructed, this returns
  // an error status.
  ClientStatus RestoreElement(const DomObjectFrameStack& object,
                              ElementFinderResult* out_element) const;

  // Removes an element. Returns true if the element was removed.
  bool RemoveElement(const std::string& client_id);

  // Check whether an element exists.
  bool HasElement(const std::string& client_id) const;

  // Clear all elements from the store.
  void Clear();

 private:
  friend class FakeElementStore;

  raw_ptr<content::WebContents> web_contents_;

  base::flat_map<std::string, DomObjectFrameStack> object_map_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_STORE_H_
