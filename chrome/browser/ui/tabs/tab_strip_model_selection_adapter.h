// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_SELECTION_ADAPTER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_SELECTION_ADAPTER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_selection_state.h"
#include "ui/base/models/list_selection_model.h"

class TabStripModel;

namespace tabs {
class TabInterface;
class TabStripModelSelectionState;
}  // namespace tabs

// An abstract base class that adapts different selection states to a common
// interface that can be used by TabStripModel.
class TabStripModelSelectionAdapter {
 public:
  virtual ~TabStripModelSelectionAdapter() = default;

  virtual void set_anchor(std::optional<size_t> anchor) = 0;
  virtual std::optional<size_t> anchor() const = 0;
  virtual void set_active(std::optional<size_t> active) = 0;
  virtual std::optional<size_t> active() const = 0;
  virtual bool empty() const = 0;
  virtual size_t size() const = 0;
  virtual void IncrementFrom(size_t index) = 0;
  virtual void DecrementFrom(size_t index,
                             tabs::TabInterface* tab_being_removed) = 0;
  virtual void SetSelectedIndex(std::optional<size_t> index) = 0;
  virtual bool IsSelected(size_t index) const = 0;
  virtual void AddIndexToSelection(size_t index) = 0;
  virtual void AddIndexRangeToSelection(size_t index_start,
                                        size_t index_end) = 0;
  virtual void RemoveIndexFromSelection(size_t index) = 0;
  virtual void SetSelectionFromAnchorTo(size_t index) = 0;
  virtual void AddSelectionFromAnchorTo(size_t index) = 0;
  virtual void Move(size_t old_index, size_t new_index, size_t length) = 0;
  virtual void Clear() = 0;
  virtual ui::ListSelectionModel ToListSelectionModel() const = 0;
  virtual ui::ListSelectionModel::SelectedIndices selected_indices() const = 0;
};

// An adapter that allows the TabStripModelSelectionState to be used like a
// ui::ListSelectionModel.
class TabStripModelSelectionStateAdapter final
    : public TabStripModelSelectionAdapter {
 public:
  explicit TabStripModelSelectionStateAdapter(TabStripModel* model);
  ~TabStripModelSelectionStateAdapter() override;

  // TabStripModelSelectionAdapter implementation
  void set_anchor(std::optional<size_t> anchor) override;
  std::optional<size_t> anchor() const override;
  void set_active(std::optional<size_t> active) override;
  std::optional<size_t> active() const override;
  bool empty() const override;
  size_t size() const override;
  void IncrementFrom(size_t index) override;
  void DecrementFrom(size_t index,
                     tabs::TabInterface* tab_being_removed) override;
  void SetSelectedIndex(std::optional<size_t> index) override;
  bool IsSelected(size_t index) const override;
  void AddIndexToSelection(size_t index) override;
  void AddIndexRangeToSelection(size_t index_start, size_t index_end) override;
  void RemoveIndexFromSelection(size_t index) override;
  void SetSelectionFromAnchorTo(size_t index) override;
  void AddSelectionFromAnchorTo(size_t index) override;
  void Move(size_t old_index, size_t new_index, size_t length) override;
  void Clear() override;
  ui::ListSelectionModel ToListSelectionModel() const override;
  ui::ListSelectionModel::SelectedIndices selected_indices() const override;

 private:
  const raw_ptr<TabStripModel> tab_strip_model_;
  tabs::TabStripModelSelectionState selection_state_;
};

// An adapter for a ui::ListSelectionModel.
class ListSelectionModelAdapter final : public TabStripModelSelectionAdapter {
 public:
  ListSelectionModelAdapter();
  explicit ListSelectionModelAdapter(const ui::ListSelectionModel& model);
  ~ListSelectionModelAdapter() override;

  // TabStripModelSelectionAdapter implementation
  void set_anchor(std::optional<size_t> anchor) override;
  std::optional<size_t> anchor() const override;
  void set_active(std::optional<size_t> active) override;
  std::optional<size_t> active() const override;
  bool empty() const override;
  size_t size() const override;
  void IncrementFrom(size_t index) override;
  void DecrementFrom(size_t index,
                     tabs::TabInterface* tab_being_removed) override;
  void SetSelectedIndex(std::optional<size_t> index) override;
  bool IsSelected(size_t index) const override;
  void AddIndexToSelection(size_t index) override;
  void AddIndexRangeToSelection(size_t index_start, size_t index_end) override;
  void RemoveIndexFromSelection(size_t index) override;
  void SetSelectionFromAnchorTo(size_t index) override;
  void AddSelectionFromAnchorTo(size_t index) override;
  void Move(size_t old_index, size_t new_index, size_t length) override;
  void Clear() override;
  ui::ListSelectionModel ToListSelectionModel() const override;
  ui::ListSelectionModel::SelectedIndices selected_indices() const override;

 private:
  ui::ListSelectionModel model_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_SELECTION_ADAPTER_H_
