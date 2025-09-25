// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_DIRECT_CHILD_WALKER_H_
#define COMPONENTS_TABS_PUBLIC_DIRECT_CHILD_WALKER_H_

#include "base/notreached.h"
#include "base/types/pass_key.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

// DirectChildWalker iterates over the direct children of a TabCollection
// and calls a processor for each child.
class DirectChildWalker {
 public:
  // The interface that callers must implement to process the children.
  class Processor {
   public:
    virtual ~Processor() = default;
    virtual void ProcessTab(const TabInterface* tab) = 0;
    virtual void ProcessCollection(const TabCollection* collection) = 0;
  };

  DirectChildWalker(const TabCollection* collection, Processor* processor)
      : collection_(collection), processor_(processor) {}

  void Walk() {
    for (const auto& child :
         collection_->GetChildren(base::PassKey<DirectChildWalker>())) {
      if (std::holds_alternative<std::unique_ptr<TabInterface>>(child)) {
        const TabInterface* tab =
            std::get<std::unique_ptr<TabInterface>>(child).get();
        processor_->ProcessTab(tab);
      } else if (std::holds_alternative<std::unique_ptr<TabCollection>>(
                     child)) {
        const TabCollection* collection =
            std::get<std::unique_ptr<TabCollection>>(child).get();
        processor_->ProcessCollection(collection);
      } else {
        NOTREACHED() << "unknown node type";
      }
    }
  }

 private:
  const raw_ptr<const TabCollection> collection_;
  const raw_ptr<Processor> processor_;
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_DIRECT_CHILD_WALKER_H_
