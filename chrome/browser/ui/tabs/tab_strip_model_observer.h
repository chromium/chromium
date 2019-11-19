// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "ui/base/models/list_selection_model.h"

class TabGroupVisualData;
class TabStripModel;

namespace content {
class WebContents;
}

////////////////////////////////////////////////////////////////////////////////
//
// TabStripModelChange / TabStripSelectionChange
//
// The following class and structures are used to inform TabStripModelObservers
// of changes to:
// 1) selection model
// 2) activated tab
// 3) inserted/removed/moved tabs.
// These changes must be bundled together because (1) and (2) consist of indices
// into a list of tabs [determined by (3)]. All three must be kept synchronized.
//
////////////////////////////////////////////////////////////////////////////////
class TabStripModelChange {
 public:
  enum Type {
    kSelectionOnly,
    kInserted,
    kRemoved,
    kMoved,
    kReplaced,
    kGroupChanged
  };

  // Base class for all changes.
  // TODO(dfried): would love to change this whole thing into a std::variant,
  // but C++17 features are not yet approved for use in chromium.
  struct Delta {
    virtual ~Delta() = default;
  };

  struct ContentsWithIndex {
    content::WebContents* contents;
    int index;
  };

  // WebContents were inserted. This implicitly changes the existing selection
  // model by calling IncrementFrom(index) on each index in |contents[i].index|.
  struct Insert : public Delta {
    Insert();
    ~Insert() override;
    Insert(Insert&& other);
    Insert& operator=(Insert&& other);

    // Contains the web contents that were inserted, along with their indexes at
    // the time of insertion. For example, if we inserted elements:
    //
    // Before insertion:
    // A B C D
    // 0 1 2 3
    //
    // After insertion:
    // A X Y B C Z D
    // 0 1 2 3 4 5 6
    //
    // If the tabs were inserted in the order X, Y, Z, |contents| would contain:
    // { X, 1 }, { Y, 2 }, { Z, 5 }
    //
    // But if the contents were inserted in the order Z, Y, X, |contents| would
    // contain:
    // { Z, 3 }, { Y, 1 }, { X, 1 }
    //
    // Therefore all observers which store indices of web contents should update
    // them in the order the web contents appear in |contents|. Observers should
    // not do index-based queries based on their own internally-stored indices
    // until after processing all of |contents|.
    std::vector<ContentsWithIndex> contents;
  };

  // WebContents were removed at |indices_before_removal|. This implicitly
  // changes the existing selection model by calling DecrementFrom(index).
  struct Remove : public Delta {
    Remove();
    ~Remove() override;
    Remove(Remove&& other);
    Remove& operator=(Remove&& other);

    // Contains the list of web contents removed, along with their indexes at
    // the time of removal. For example, if we removed elements:
    //
    // Before removal:
    // A B C D E F G
    // 0 1 2 3 4 5 6
    //
    // After removal:
    // A D E G
    // 0 1 2 3
    //
    // If the tabs were removed in the order B, C, F, |contents| would contain:
    // { B, 1 }, { C, 1 }, { F, 3 }
    //
    // But if the tabs were removed in the order F, C, B, then |contents| would
    // contain:
    // { F, 5 }, { C, 2 }, { B, 1 }
    //
    // Therefore all observers which store indices of web contents should update
    // them in the order the web contents appear in |contents|. Observers should
    // not do index-based queries based on their own internally-stored indices
    // until after processing all of |contents|.
    std::vector<ContentsWithIndex> contents;

    // The specified WebContents are being closed (and eventually destroyed).
    // |tab_strip_model| is the TabStripModel that contained the tab.
    bool will_be_deleted;
  };

  // A WebContents was moved from |from_index| to |to_index|. This implicitly
  // changes the existing selection model by calling
  // Move(from_index, to_index, 1).
  struct Move : public Delta {
    content::WebContents* contents;
    int from_index;
    int to_index;
  };

  // The WebContents was replaced at the specified index. This is invoked when
  // prerendering swaps in a prerendered WebContents.
  struct Replace : public Delta {
    content::WebContents* old_contents;
    content::WebContents* new_contents;
    int index;
  };

  // A WebContents' group affiliation changed from |old_group| to |new_group|.
  struct GroupChange : public Delta {
    // Constructors and destructor required due to Optional.
    GroupChange();
    GroupChange(const GroupChange& other);
    GroupChange& operator=(const GroupChange& other);
    ~GroupChange() override;

    content::WebContents* contents;
    int index;
    base::Optional<TabGroupId> old_group;
    base::Optional<TabGroupId> new_group;
  };

  TabStripModelChange();
  explicit TabStripModelChange(Insert delta);
  explicit TabStripModelChange(Remove delta);
  explicit TabStripModelChange(Replace delta);
  explicit TabStripModelChange(Move delta);
  explicit TabStripModelChange(GroupChange delta);
  ~TabStripModelChange();

  Type type() const { return type_; }
  const Insert* GetInsert() const;
  const Remove* GetRemove() const;
  const Move* GetMove() const;
  const Replace* GetReplace() const;
  const GroupChange* GetGroupChange() const;

 private:
  TabStripModelChange(Type type, std::unique_ptr<Delta> delta);

  const Type type_ = kSelectionOnly;
  std::unique_ptr<Delta> delta_;

  DISALLOW_COPY_AND_ASSIGN(TabStripModelChange);
};

// Struct to carry changes on selection/activation.
struct TabStripSelectionChange {
  TabStripSelectionChange();
  TabStripSelectionChange(const TabStripSelectionChange& other);
  ~TabStripSelectionChange();

  TabStripSelectionChange& operator=(const TabStripSelectionChange& other);

  // Fill TabStripSelectionChange with given |contents| and |selection_model|.
  // note that |new_contents| and |new_model| will be filled too so that
  // selection_changed() and active_tab_changed() won't return true.
  TabStripSelectionChange(content::WebContents* contents,
                          const ui::ListSelectionModel& model);

  bool active_tab_changed() const { return old_contents != new_contents; }

  // TODO(sangwoo.ko) Do we need something to indicate that the change
  // was made implicitly?
  bool selection_changed() const {
    return selected_tabs_were_removed || old_model != new_model;
  }

  content::WebContents* old_contents = nullptr;
  content::WebContents* new_contents = nullptr;

  ui::ListSelectionModel old_model;
  ui::ListSelectionModel new_model;

  bool selected_tabs_were_removed = false;

  int reason = 0;
};

////////////////////////////////////////////////////////////////////////////////
//
// TabStripModelObserver
//
//  Objects implement this interface when they wish to be notified of changes
//  to the TabStripModel.
//
//  Two major implementers are the TabStrip, which uses notifications sent
//  via this interface to update the presentation of the strip, and the Browser
//  object, which updates bookkeeping and shows/hides individual WebContentses.
//
//  Register your TabStripModelObserver with the TabStripModel using its
//  Add/RemoveObserver methods.
//
////////////////////////////////////////////////////////////////////////////////
class TabStripModelObserver {
 public:
  enum ChangeReason {
    // Used to indicate that none of the reasons below are responsible for the
    // active tab change.
    CHANGE_REASON_NONE = 0,
    // The active tab changed because the tab's web contents was replaced.
    CHANGE_REASON_REPLACED = 1 << 0,
    // The active tab changed due to a user input event.
    CHANGE_REASON_USER_GESTURE = 1 << 1,
  };

  enum CloseAllStoppedReason {
    // Used to indicate that CloseAllTab event is canceled.
    kCloseAllCanceled = 0,
    // Used to indicate that CloseAllTab event complete successfully.
    kCloseAllCompleted = 1,
  };

  // |change| is a series of changes in tabtrip model. |change| consists
  // of changes with same type and those changes may have caused selection or
  // activation changes. |selection| is determined by comparing the state of
  // TabStripModel before the |change| and after the |change| are applied.
  // When only selection/activation was changed without any change about
  // WebContents, |change| can be empty.
  virtual void OnTabStripModelChanged(TabStripModel* tab_strip_model,
                                      const TabStripModelChange& change,
                                      const TabStripSelectionChange& selection);

  // Called when the TabGroupVisualData associated with a group changes. |group|
  // identifies which group changed. |visual_data| is a pointer to the new
  // TabGroupVisualData and is valid until either the data associated with
  // |group| changes again or |group| is destroyed.
  virtual void OnTabGroupVisualDataChanged(
      TabStripModel* tab_strip_model,
      TabGroupId group,
      const TabGroupVisualData* visual_data);

  // The specified WebContents at |index| changed in some way. |contents|
  // may be an entirely different object and the old value is no longer
  // available by the time this message is delivered.
  //
  // See tab_change_type.h for a description of |change_type|.
  virtual void TabChangedAt(content::WebContents* contents,
                            int index,
                            TabChangeType change_type);

  // Invoked when the pinned state of a tab changes.
  virtual void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                                     content::WebContents* contents,
                                     int index);

  // Invoked when the blocked state of a tab changes.
  // NOTE: This is invoked when a tab becomes blocked/unblocked by a tab modal
  // window.
  virtual void TabBlockedStateChanged(content::WebContents* contents,
                                      int index);

  // The TabStripModel now no longer has any tabs. The implementer may
  // use this as a trigger to try and close the window containing the
  // TabStripModel, for example...
  virtual void TabStripEmpty();

  // Sent any time an attempt is made to close all the tabs. This is not
  // necessarily the result of CloseAllTabs(). For example, if the user closes
  // the last tab WillCloseAllTabs() is sent. If the close does not succeed
  // during the current event (say unload handlers block it) then
  // CloseAllTabsStopped() is sent with reason 'CANCELED'. On the other hand if
  // the close does finish then CloseAllTabsStopped() is sent with reason
  // 'COMPLETED'. Also note that if the last tab is detached
  // (DetachWebContentsAt()) then this is not sent.
  virtual void WillCloseAllTabs(TabStripModel* tab_strip_model);
  virtual void CloseAllTabsStopped(TabStripModel* tab_strip_model,
                                   CloseAllStoppedReason reason);

  // The specified tab at |index| requires the display of a UI indication to the
  // user that it needs their attention. The UI indication is set iff
  // |attention| is true.
  virtual void SetTabNeedsAttentionAt(int index, bool attention);

  // Called when an observed TabStripModel is beginning destruction.
  virtual void OnTabStripModelDestroyed(TabStripModel* tab_strip_model);

  static void StopObservingAll(TabStripModelObserver* observer);
  static bool IsObservingAny(TabStripModelObserver* observer);
  static int CountObservedModels(TabStripModelObserver* observer);

  // A passkey for TabStripModel to access some methods on this class - see
  // </docs/patterns/passkey.md>.
  class ModelPasskey {
   private:
    friend class TabStripModel;
    ModelPasskey() = default;
    ~ModelPasskey() = default;
  };

  // These methods are used by TabStripModel to notify this class of lifecycle
  // events on the TabStripModelObserver or the TabStripModel itself. The first
  // two are used to allow TabStripModelObserver to track which models it is
  // observing. The third is used to allow TabStripModelObserver to clean up
  // when an observed TabStripModel is destroyed, and to send the
  // OnTabStripModelDestroyed notification above.
  void StartedObserving(ModelPasskey, TabStripModel* model);
  void StoppedObserving(ModelPasskey, TabStripModel* model);
  void ModelDestroyed(ModelPasskey, TabStripModel* model);

 protected:
  TabStripModelObserver();
  virtual ~TabStripModelObserver();

 private:
  std::set<TabStripModel*> observed_models_;

  DISALLOW_COPY_AND_ASSIGN(TabStripModelObserver);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_OBSERVER_H_
