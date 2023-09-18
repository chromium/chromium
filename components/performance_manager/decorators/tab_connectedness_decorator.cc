// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/tab_connectedness_decorator.h"

#include <set>

#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

constexpr int kMaxSearchDepth = 10;

class TabConnectednessData : public NodeAttachedDataImpl<TabConnectednessData> {
 public:
  struct Traits : public NodeAttachedDataInMap<PageNodeImpl> {};

  ~TabConnectednessData() override = default;

  static TabConnectednessData* Create(TabPageDecorator::TabHandle* tab_handle) {
    auto* data = TabConnectednessData::GetOrCreate(
        PageNodeImpl::FromNode(tab_handle->page_node()));
    CHECK(!data->tab_handle_);
    data->SetTabHandle(tab_handle);
    return data;
  }

  // Recursively compute the connectedness from this tab to `destination`,
  // stopping after `depth` reaches `kMaxSearchDepth`. It's not relevant to know
  // about a tab that's "connected" to the destination by a large amount of
  // hops, since each hop represents a user-initiated tab switch. Capping the
  // search depth to be relatively shallow like this allows a few
  // simplifications:
  //
  // 1- The search doesn't need to account for cycles, since being stuck in a
  // cycle for a few iterations isn't problematic when there is a guarantee that
  // the cycle will be broken eventually.
  //
  // 2- The search doesn't need to explicitly handle the case where the
  // connectedness graph is large but disjoint and the source and destination
  // are in separate partitions, since the search will naturally be aborted
  // after a certain depth is reached.
  float ComputeConnectednessTo(TabPageDecorator::TabHandle* destination,
                               int depth = 0) const {
    if (destination == tab_handle_) {
      // TabHandles are unique and stable for the lifetime of the tab. If this
      // is attached to the destination tab, return 1.
      return 1.0f;
    }

    if (depth == kMaxSearchDepth) {
      return 0.0f;
    }

    float connectedness = 0.0f;
    for (const auto& [other_tab_handle, switch_count] : switch_counts_) {
      TabConnectednessData* data = TabConnectednessData::Get(
          PageNodeImpl::FromNode(other_tab_handle->page_node()));
      connectedness += ((static_cast<float>(switch_count) /
                         static_cast<float>(total_switch_count_)) *
                        data->ComputeConnectednessTo(destination, depth + 1));
    }

    // It's possible to get here with a connectedness score greater than 1 due
    // to floating point error accumulation. If that's the case, the true value
    // is close enough to 1 that we can return 1 directly.
    return std::min(connectedness, 1.0f);
  }

  void OnSwitchTo(const TabPageDecorator::TabHandle* other) {
    // It's possible that `switch_counts_` doesn't already contain an entry for
    // `other`, in which case `operator[]` will create such an entry with `0.0f`
    // for the value.
    auto& count = switch_counts_[other];

    if (count == 0) {
      // Counts don't decrease, so the count being 0 means operator[] just
      // inserted the element in the map.
      TabConnectednessData* other_data =
          TabConnectednessData::Get(PageNodeImpl::FromNode(other->page_node()));
      CHECK(other_data);
      // Tell the other node that this one is connected to it so this one can be
      // notified when the other one is destroyed.
      other_data->BecomeConnectionTarget(tab_handle_);
    }

    count += 1;
    total_switch_count_ += 1;
  }

  void OnWillBeRemoved() {
    if (!tab_handle_) {
      // if `tab_handle_` is nullptr, it means this is attached to a PageNode
      // that was discarded. The data associated with `this` was already moved
      // to a new TabConnectednessData, so there's nothing to do.
      return;
    }

    // For all the tabs this one is connected to, notify them that they should
    // not try to notify this one when they are deleted (since this one will be
    // gone by then).
    for (const auto& [other_tab_handle, _] : switch_counts_) {
      TabConnectednessData* other_data = TabConnectednessData::Get(
          PageNodeImpl::FromNode(other_tab_handle->page_node()));
      CHECK(other_data);

      other_data->SeverConnectionFrom(tab_handle_);
    }

    // For all the other tabs connected to this one, notify them that they
    // should get rid of counts related to this one.
    for (auto* other_tab_handle : tabs_connected_to_this_one_) {
      TabConnectednessData* other_data = TabConnectednessData::Get(
          PageNodeImpl::FromNode(other_tab_handle->page_node()));
      other_data->RemoveFromCounts(tab_handle_);
    }

    // Set `tab_handle_` to null to avoid leaving it dangling, which can trip up
    // the dangling pointer detector if the `TabHandle` is deleted before the
    // `TabConnectednessData`. This can happen when because the destruction
    // order of `NodeAttachedData` is unspecified, but when we reach this point
    // neither object will be used anymore before being deleted.
    tab_handle_ = nullptr;
  }

  void OnWillBeReplaced(TabConnectednessData* other) {
    // Transfer other data members.
    other->switch_counts_ = std::move(switch_counts_);
    other->total_switch_count_ = total_switch_count_;
    other->tabs_connected_to_this_one_ = std::move(tabs_connected_to_this_one_);

    // Set `tab_handle_` to nullptr to signify that this data has been moved out
    // and there's no cleanup to do.
    tab_handle_ = nullptr;
  }

 private:
  friend class NodeAttachedDataImpl<TabConnectednessData>;
  friend class NodeAttachedDataImpl<
      TabConnectednessData>::NodeAttachedDataInMap<PageNodeImpl>;

  explicit TabConnectednessData(const PageNodeImpl* page_node) {}

  void SetTabHandle(TabPageDecorator::TabHandle* tab_handle) {
    tab_handle_ = tab_handle;
  }

  // Invoked when `source_tab` first becomes connected to this one, i.e. when
  // the first tab switch from `source_tab` to this tab happens. This is to
  // allow this tab to notify `source_tab` when this tab is being deleted.
  void BecomeConnectionTarget(const TabPageDecorator::TabHandle* source_tab) {
    auto inserted = tabs_connected_to_this_one_.insert(source_tab);
    // It shouldn't be possible for the same `TabHandle` to be inserted twice.
    CHECK(inserted.second);
  }

  // Invoked when `source_tab`, currently connected to this one is going away,
  // signifying that we shouldn't attempt to notify it of our own destruction
  // when that time comes.
  void SeverConnectionFrom(const TabPageDecorator::TabHandle* source_tab) {
    size_t erase_count = tabs_connected_to_this_one_.erase(source_tab);
    // Attempting to remove a connection that wasn't created in the first place
    // would indicate a wider bug, so CHECK that there actually was an element
    // removed.
    CHECK_EQ(erase_count, 1U);
  }

  // Invoked when `tab` is being deleted, signifying that we should remove it
  // from our count information.
  void RemoveFromCounts(const TabPageDecorator::TabHandle* tab) {
    auto it = switch_counts_.find(tab);
    CHECK(it != switch_counts_.end());
    total_switch_count_ -= it->second;
    switch_counts_.erase(it);
  }

  // For each entry {K,V} in this map, V is the amount of times the user
  // switched from this tab to tab K.
  std::map<const TabPageDecorator::TabHandle*, uint32_t> switch_counts_;

  // A set of TabHandles representing tabs that are connected to this one, i.e.
  // have this tab's TabHandle in their `switch_counts_` map. This is used when
  // this tab is being deleted to notify those other tabs that they should
  // remove it and its associated counts from their internal members.
  std::set<const TabPageDecorator::TabHandle*> tabs_connected_to_this_one_;

  // The total amount of times the user switched from this tab to any other.
  // This is equal to the sum of all of the values in `switch_counts_`.
  uint32_t total_switch_count_{0};

  raw_ptr<TabPageDecorator::TabHandle> tab_handle_{nullptr};
};

TabConnectednessDecorator::TabConnectednessDecorator() = default;
TabConnectednessDecorator::~TabConnectednessDecorator() = default;

// static
float TabConnectednessDecorator::ComputeConnectednessBetween(
    TabPageDecorator::TabHandle* source,
    TabPageDecorator::TabHandle* destination) {
  TabConnectednessData* data =
      TabConnectednessData::Get(PageNodeImpl::FromNode(source->page_node()));
  CHECK(data);

  return data->ComputeConnectednessTo(destination);
}

void TabConnectednessDecorator::AddObserver(Observer* o) {
  observers_.AddObserver(o);
}

void TabConnectednessDecorator::RemoveObserver(Observer* o) {
  observers_.RemoveObserver(o);
}

// static
void TabConnectednessDecorator::NotifyOfTabSwitch(content::WebContents* from,
                                                  content::WebContents* to) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> from, base::WeakPtr<PageNode> to,
             performance_manager::Graph* graph) {
            if (from && to) {
              TabConnectednessDecorator* decorator =
                  graph->GetRegisteredObjectAs<TabConnectednessDecorator>();
              // Some unit tests exercise tab switch code paths on the UI thread
              // and don't setup a full PM or handle the sequence hops involved.
              // In these tests, do nothing here.
              if (decorator) {
                decorator->OnTabSwitch(from.get(), to.get());
              }
            }
          },
          PerformanceManager::GetPrimaryPageNodeForWebContents(from),
          PerformanceManager::GetPrimaryPageNodeForWebContents(to)));
}

void TabConnectednessDecorator::OnTabSwitch(const PageNode* from,
                                            const PageNode* to) {
  CHECK(from);
  CHECK(to);
  TabPageDecorator::TabHandle* to_tab_handle =
      TabPageDecorator::FromPageNode(to);
  TabPageDecorator::TabHandle* from_tab_handle =
      TabPageDecorator::FromPageNode(from);
  // It shouldn't be possible for a non-tab page to be switched to.
  CHECK(to_tab_handle);
  // It is, however, possible for a "tab switch" to originate from a non-tab
  // `PageNode` in browser tests. This happens when test utils are used to
  // discard the only tab in the browser and then ui_test_utils::NavigateToURL
  // is used to reload it. This doesn't happen in the wild because only
  // background tabs get discarded.
  if (!from_tab_handle) {
    return;
  }

  for (auto& o : observers_) {
    o.OnBeforeTabSwitch(from_tab_handle, to_tab_handle);
  }

  TabConnectednessData* data =
      TabConnectednessData::Get(PageNodeImpl::FromNode(from));
  CHECK(data);

  data->OnSwitchTo(to_tab_handle);
}

void TabConnectednessDecorator::OnTabAdded(
    TabPageDecorator::TabHandle* tab_handle) {
  // Create the data here so that all other call sites can call `Get()` and
  // `CHECK` that there is already data attached.
  TabConnectednessData::Create(tab_handle);
}

void TabConnectednessDecorator::OnTabAboutToBeDiscarded(
    const PageNode* old_page_node,
    TabPageDecorator::TabHandle* tab_handle) {
  // Create a data object for the new node and transfer the data from the old
  // one into it.
  TabConnectednessData* new_data = TabConnectednessData::Create(tab_handle);
  TabConnectednessData* old_data =
      TabConnectednessData::Get(PageNodeImpl::FromNode(old_page_node));

  old_data->OnWillBeReplaced(new_data);
}

void TabConnectednessDecorator::OnBeforeTabRemoved(
    TabPageDecorator::TabHandle* tab_handle) {
  TabConnectednessData* data = TabConnectednessData::Get(
      PageNodeImpl::FromNode(tab_handle->page_node()));
  CHECK(data);

  data->OnWillBeRemoved();
}

void TabConnectednessDecorator::OnPassedToGraph(Graph* graph) {
  graph->RegisterObject(this);
  graph->GetRegisteredObjectAs<TabPageDecorator>()->AddObserver(this);
}

void TabConnectednessDecorator::OnTakenFromGraph(Graph* graph) {
  graph->UnregisterObject(this);
  // GraphOwned object destruction order is undefined, so only remove ourselves
  // as observers if the decorator still exists.
  TabPageDecorator* tab_page_decorator =
      graph->GetRegisteredObjectAs<TabPageDecorator>();
  if (tab_page_decorator) {
    tab_page_decorator->RemoveObserver(this);
  }
}

}  // namespace performance_manager
