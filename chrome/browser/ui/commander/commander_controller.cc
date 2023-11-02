// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/commander_controller.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/commander/bookmark_command_source.h"
#include "chrome/browser/ui/commander/command_source.h"
#include "chrome/browser/ui/commander/commander_view_model.h"
#include "chrome/browser/ui/commander/open_url_command_source.h"
#include "chrome/browser/ui/commander/simple_command_source.h"
#include "chrome/browser/ui/commander/tab_command_source.h"
#include "chrome/browser/ui/commander/window_command_source.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace commander {

namespace {

size_t constexpr kMaxResults = 8;

CommanderController::CommandSources CreateDefaultSources() {
  CommanderController::CommandSources sources;
  sources.push_back(std::make_unique<SimpleCommandSource>());
  sources.push_back(std::make_unique<OpenURLCommandSource>());
  sources.push_back(std::make_unique<BookmarkCommandSource>());
  sources.push_back(std::make_unique<WindowCommandSource>());
  sources.push_back(std::make_unique<TabCommandSource>());
  return sources;
}

}  // namespace

CommanderController::CommanderController()
    : CommanderController(CreateDefaultSources()) {}

CommanderController::CommanderController(CommandSources sources)
    : current_result_set_id_(0), sources_(std::move(sources)) {}

CommanderController::~CommanderController() = default;

void CommanderController::OnTextChanged(const std::u16string& text,
                                        Browser* browser) {
  std::vector<std::unique_ptr<CommandItem>> items;
  if (composite_command_provider_) {
    items = composite_command_provider_.Run(text);
  } else {
    for (auto& source : sources_) {
      auto commands = source->GetCommands(text, browser);
      items.insert(items.end(), std::make_move_iterator(commands.begin()),
                   std::make_move_iterator(commands.end()));
    }
  }

  // Sort descending by score, then alphabetically.
  size_t max_elements = std::min(items.size(), kMaxResults);
  std::partial_sort(
      std::begin(items), std::begin(items) + max_elements, std::end(items),
      [](const std::unique_ptr<CommandItem>& left,
         const std::unique_ptr<CommandItem>& right) {
        return (left->score == right->score) ? left->title < right->title
                                             : left->score > right->score;
      });
  if (items.size() > kMaxResults)
    items.resize(kMaxResults);
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
  if (command_index >= current_items_.size() ||
      result_set_id != current_result_set_id_)
    return;

  CommandItem* item = current_items_[command_index].get();
  if (item->GetType() == CommandItem::Type::kOneShot) {
    base::OnceClosure command =
        std::move(absl::get<base::OnceClosure>(item->command));
    // Dismiss the view.
    CommanderViewModel vm;
    vm.result_set_id = ++current_result_set_id_;
    vm.action = CommanderViewModel::Action::kClose;
    callback_.Run(vm);

    std::move(command).Run();
  } else {
    CommandItem::CompositeCommand command =
        absl::get<CommandItem::CompositeCommand>(item->command);
    composite_command_provider_ = command.second;
    // Tell the view to requery.
    CommanderViewModel vm;
    vm.result_set_id = ++current_result_set_id_;
    vm.action = CommanderViewModel::Action::kPrompt;
    vm.prompt_text = command.first;
    callback_.Run(vm);
  }
}

void CommanderController::OnCompositeCommandCancelled() {
  DCHECK(composite_command_provider_);
  composite_command_provider_.Reset();
}

void CommanderController::SetUpdateCallback(ViewModelUpdateCallback callback) {
  callback_ = std::move(callback);
}

void CommanderController::Reset() {
  current_items_.clear();
  if (composite_command_provider_)
    composite_command_provider_.Reset();
}

// static
std::unique_ptr<CommanderController>
CommanderController::CreateWithSourcesForTesting(CommandSources sources) {
  auto* instance = new CommanderController(std::move(sources));
  return base::WrapUnique(instance);
}

}  // namespace commander
