// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/commander_controller.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/commander/commander_view_model.h"

namespace commander {

namespace {

CommanderController::CommandSources CreateDefaultSources() {
  return {};
}

}  // namespace

CommanderController::CommanderController()
    : CommanderController(CreateDefaultSources()) {}

CommanderController::CommanderController(CommandSources sources)
    : current_result_set_id_(0), sources_(std::move(sources)) {}

CommanderController::~CommanderController() = default;

void CommanderController::OnDelegateViewModelCallback(
    CommanderViewModel view_model) {
  view_model.result_set_id = ++current_result_set_id_;
  callback_.Run(view_model);
}

void CommanderController::OnTextChanged(const base::string16& text,
                                        Browser* browser) {
  if (delegate_.get())
    return delegate_->OnTextChanged(text, browser);

  std::vector<std::unique_ptr<CommandItem>> items;
  for (auto& source : sources_) {
    auto commands = source->GetCommands(text, browser);
    items.insert(items.end(), std::make_move_iterator(commands.begin()),
                 std::make_move_iterator(commands.end()));
  }

  // Just sort for now.
  std::sort(std::begin(items), std::end(items),
            [](const std::unique_ptr<CommandItem>& left,
               const std::unique_ptr<CommandItem>& right) {
              return left->score > right->score;
            });
  // TODO(lgrey): Threshold this at some kind of max items.
  current_items_ = std::move(items);
  CommanderViewModel vm;
  vm.result_set_id = ++current_result_set_id_;
  vm.action = CommanderViewModel::Action::kDisplayResults;
  for (auto& item : current_items_) {
    vm.items.emplace_back(*item);
  }
  callback_.Run(vm);
}

void CommanderController::OnCommandSelected(size_t command_index,
                                            int result_set_id) {
  if (delegate_.get())
    return delegate_->OnCommandSelected(command_index, result_set_id);
  if (command_index >= current_items_.size() ||
      result_set_id != current_result_set_id_)
    return;

  CommandItem* item = current_items_[command_index].get();
  if (item->GetType() == CommandItem::Type::kOneShot) {
    base::OnceClosure command = std::move(*(item->command));
    // Dismiss the view.
    CommanderViewModel vm;
    vm.result_set_id = ++current_result_set_id_;
    vm.action = CommanderViewModel::Action::kClose;
    callback_.Run(vm);

    std::move(command).Run();
  } else {
    delegate_ = std::move(*(item->delegate_factory)).Run();
    // base::Unretained is safe since we own |delegate_|.
    delegate_->SetUpdateCallback(
        base::BindRepeating(&CommanderController::OnDelegateViewModelCallback,
                            base::Unretained(this)));
    // Tell the view to requery.
    CommanderViewModel vm;
    vm.result_set_id = ++current_result_set_id_;
    vm.action = CommanderViewModel::Action::kPrompt;
    callback_.Run(vm);
  }
}

void CommanderController::SetUpdateCallback(ViewModelUpdateCallback callback) {
  callback_ = std::move(callback);
}

void CommanderController::Reset() {
  current_items_.clear();
}

// static
std::unique_ptr<CommanderController>
CommanderController::CreateWithSourcesForTesting(CommandSources sources) {
  auto* instance = new CommanderController(std::move(sources));
  return base::WrapUnique(instance);
}

}  // namespace commander
