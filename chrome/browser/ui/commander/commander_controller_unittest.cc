// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/commander_controller.h"

#include <string>
#include <tuple>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/commander/command_source.h"
#include "chrome/browser/ui/commander/commander_view_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"

namespace commander {

namespace {

class TestCommandSource : public CommandSource {
 public:
  using GetCommandsHandler =
      base::RepeatingCallback<CommandResults(const std::u16string&,
                                             Browser* browser)>;
  explicit TestCommandSource(GetCommandsHandler handler)
      : handler_(std::move(handler)) {}
  ~TestCommandSource() override = default;

  CommandResults GetCommands(const std::u16string& input,
                             Browser* browser) const override {
    invocations_.push_back(input);
    return handler_.Run(input, browser);
  }

  const std::vector<std::u16string>& invocations() const {
    return invocations_;
  }

 private:
  mutable std::vector<std::u16string> invocations_;
  GetCommandsHandler handler_;
};

std::unique_ptr<TestCommandSource> CreateNoOpCommandSource() {
  return std::make_unique<TestCommandSource>(base::BindRepeating(
      [](const std::u16string&,
         Browser* browser) -> CommandSource::CommandResults { return {}; }));
}

std::unique_ptr<CommandItem> CreateNoOpCommandItem(const std::u16string& title,
                                                   double score) {
  std::vector<gfx::Range> ranges{{0, static_cast<uint32_t>(title.size())}};
  auto item = std::make_unique<CommandItem>(title, score, ranges);
  item->command = base::DoNothing();
  return item;
}

TestCommandSource* AddSource(
    std::vector<std::unique_ptr<CommandSource>>* sources,
    std::unique_ptr<TestCommandSource> source) {
  TestCommandSource* bare_ptr = source.get();
  sources->push_back(std::move(source));
  return bare_ptr;
}

}  // namespace

class CommanderControllerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    expected_count_ = 0;
  }

  void ExpectViewModelCallbackCalls(int expected_count) {
    expected_count_ += expected_count;
  }
  void WaitForExpectedCallbacks() {
    if (expected_count_ <= 0)
      return;
    if (!run_loop_.get() || !run_loop_->running()) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
  }
  void OnViewModelUpdated(CommanderViewModel view_model) {
    received_view_models_.push_back(view_model);
    if (expected_count_ > 0) {
      expected_count_--;
      if (run_loop_.get() && expected_count_ == 0)
        run_loop_->Quit();
    }
  }

 protected:
  std::unique_ptr<base::RunLoop> run_loop_;
  int expected_count_;
  std::vector<CommanderViewModel> received_view_models_;
};

class ViewModelCallbackWaiter {
 public:
  explicit ViewModelCallbackWaiter(CommanderControllerTest* test, int count = 1)
      : test_(test) {
    test_->ExpectViewModelCallbackCalls(count);
  }

  ~ViewModelCallbackWaiter() { test_->WaitForExpectedCallbacks(); }

 private:
  raw_ptr<CommanderControllerTest> test_;
};

TEST_F(CommanderControllerTest, PassesInputToCommandSourcesOnTextChanged) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  TestCommandSource* first = AddSource(&sources, CreateNoOpCommandSource());
  TestCommandSource* second = AddSource(&sources, CreateNoOpCommandSource());

  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  EXPECT_EQ(first->invocations().size(), 0u);
  EXPECT_EQ(second->invocations().size(), 0u);

  std::u16string input = u"foobar";
  controller->OnTextChanged(input, browser());

  EXPECT_EQ(first->invocations().size(), 1u);
  EXPECT_EQ(second->invocations().size(), 1u);

  EXPECT_EQ(first->invocations().back(), input);
  EXPECT_EQ(second->invocations().back(), input);
}

TEST_F(CommanderControllerTest, ResultSetIdsDifferAcrossCalls) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  std::ignore = AddSource(&sources, CreateNoOpCommandSource());
  base::RunLoop run_loop;
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));
  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"foobar", browser());
  }
  // Assert since we're accessing an element.
  ASSERT_EQ(received_view_models_.size(), 1u);
  int first_id = received_view_models_.back().result_set_id;

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"barfoo", browser());
  }
  EXPECT_EQ(received_view_models_.size(), 2u);
  EXPECT_NE(received_view_models_.back().result_set_id, first_id);
}

TEST_F(CommanderControllerTest, ViewModelAggregatesResults) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  auto first = std::make_unique<TestCommandSource>(
      base::BindRepeating([](const std::u16string&, Browser* browser) {
        CommandSource::CommandResults result;
        result.push_back(CreateNoOpCommandItem(u"first", 100));
        return result;
      }));
  auto second = std::make_unique<TestCommandSource>(
      base::BindRepeating([](const std::u16string&, Browser* browser) {
        CommandSource::CommandResults result;
        auto item = CreateNoOpCommandItem(u"second", 99);
        item->annotation = u"2nd";
        item->entity_type = CommandItem::Entity::kBookmark;
        result.push_back(std::move(item));
        return result;
      }));
  sources.push_back(std::move(first));
  sources.push_back(std::move(second));

  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"foobar", browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  CommanderViewModel model = received_view_models_.back();
  ASSERT_EQ(model.items.size(), 2u);

  EXPECT_EQ(model.items[0].title, u"first");
  EXPECT_EQ(model.items[0].annotation, std::u16string());
  EXPECT_EQ(model.items[0].entity_type, CommandItem::Entity::kCommand);

  EXPECT_EQ(model.items[1].title, u"second");
  EXPECT_EQ(model.items[1].annotation, u"2nd");
  EXPECT_EQ(model.items[1].entity_type, CommandItem::Entity::kBookmark);
}

// TODO(lgrey): This will need to change when scoring gets more sophisticated
// than a simple sort.
TEST_F(CommanderControllerTest, ViewModelSortsResults) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  auto first = std::make_unique<TestCommandSource>(
      base::BindRepeating([](const std::u16string&, Browser* browser) {
        CommandSource::CommandResults result;
        result.push_back(CreateNoOpCommandItem(u"third", 98));
        result.push_back(CreateNoOpCommandItem(u"first", 100));
        result.push_back(CreateNoOpCommandItem(u"fourth", 90));

        return result;
      }));
  auto second = std::make_unique<TestCommandSource>(
      base::BindRepeating([](const std::u16string&, Browser* browser) {
        CommandSource::CommandResults result;
        result.push_back(CreateNoOpCommandItem(u"second", 99));
        result.push_back(CreateNoOpCommandItem(u"fifth", 1));
        return result;
      }));
  sources.push_back(std::move(first));
  sources.push_back(std::move(second));

  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"foobar", browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  CommanderViewModel model = received_view_models_.back();
  ASSERT_EQ(model.items.size(), 5u);
  EXPECT_EQ(model.items[0].title, u"first");
  EXPECT_EQ(model.items[1].title, u"second");
  EXPECT_EQ(model.items[2].title, u"third");
  EXPECT_EQ(model.items[3].title, u"fourth");
  EXPECT_EQ(model.items[4].title, u"fifth");
}

TEST_F(CommanderControllerTest, ViewModelSortsSameScoreAlphabetically) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  auto source = std::make_unique<TestCommandSource>(
      base::BindRepeating([](const std::u16string&, Browser* browser) {
        CommandSource::CommandResults result;
        result.push_back(CreateNoOpCommandItem(u"clementine", 100));
        result.push_back(CreateNoOpCommandItem(u"apple", 100));
        result.push_back(CreateNoOpCommandItem(u"banana", 100));

        return result;
      }));
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"foobar", browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  CommanderViewModel model = received_view_models_.back();
  ASSERT_EQ(model.items.size(), 3u);
  EXPECT_EQ(model.items[0].title, u"apple");
  EXPECT_EQ(model.items[1].title, u"banana");
  EXPECT_EQ(model.items[2].title, u"clementine");
}

TEST_F(CommanderControllerTest, ViewModelRetainsBoldRanges) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  auto source = std::make_unique<TestCommandSource>(
      base::BindRepeating([=](const std::u16string&, Browser* browser) {
        auto first = CreateNoOpCommandItem(u"first", 100);
        auto second = CreateNoOpCommandItem(u"second", 99);
        first->matched_ranges.clear();
        first->matched_ranges.emplace_back(0, 2);
        first->matched_ranges.emplace_back(4, 1);
        second->matched_ranges.clear();
        second->matched_ranges.emplace_back(1, 4);
        CommandSource::CommandResults result;
        result.push_back(std::move(first));
        result.push_back(std::move(second));
        return result;
      }));
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"foobar", browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  CommanderViewModel model = received_view_models_.back();
  // Ensure |first| is at index 0;
  EXPECT_EQ(model.items[0].title, u"first");
  std::vector<gfx::Range> first_ranges = {gfx::Range(0, 2), gfx::Range(4, 1)};
  std::vector<gfx::Range> second_ranges = {gfx::Range(1, 4)};
  EXPECT_EQ(model.items[0].matched_ranges, first_ranges);
  EXPECT_EQ(model.items[1].matched_ranges, second_ranges);
}

TEST_F(CommanderControllerTest, OnCommandSelectedInvokesOneShotCommand) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  bool first_called = false;
  bool second_called = false;
  auto source = std::make_unique<TestCommandSource>(base::BindRepeating(
      [](bool* first_called_ptr, bool* second_called_ptr, const std::u16string&,
         Browser* browser) {
        auto first = CreateNoOpCommandItem(u"first", 100);
        auto second = CreateNoOpCommandItem(u"second", 99);
        first->command = base::BindOnce([](bool* called) { *called = true; },
                                        first_called_ptr);
        second->command = base::BindOnce([](bool* called) { *called = true; },
                                         second_called_ptr);
        CommandSource::CommandResults result;
        result.push_back(std::move(first));
        result.push_back(std::move(second));
        return result;
      },
      &first_called, &second_called));
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"foobar", browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  CommanderViewModel model = received_view_models_.back();
  // Ensure |first| is at index 0;
  EXPECT_EQ(model.items[0].title, u"first");

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnCommandSelected(0, model.result_set_id);
  }
  EXPECT_TRUE(first_called);
  EXPECT_FALSE(second_called);
  EXPECT_EQ(received_view_models_.size(), 2u);
  EXPECT_EQ(received_view_models_.back().action,
            CommanderViewModel::Action::kClose);
}

TEST_F(CommanderControllerTest, NoActionOnIncorrectResultId) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  bool item_called = false;
  auto source = std::make_unique<TestCommandSource>(base::BindRepeating(
      [](bool* called_ptr, const std::u16string&, Browser* browser) {
        auto item = CreateNoOpCommandItem(u"first", 100);
        item->command =
            base::BindOnce([](bool* called) { *called = true; }, called_ptr);
        CommandSource::CommandResults result;
        result.push_back(std::move(item));
        return result;
      },
      &item_called));
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"foobar", browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  CommanderViewModel model = received_view_models_.back();

  controller->OnCommandSelected(0, model.result_set_id - 1);

  EXPECT_FALSE(item_called);
}

TEST_F(CommanderControllerTest, NoActionOnOOBIndex) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  bool item_called = false;
  auto source = std::make_unique<TestCommandSource>(base::BindRepeating(
      [](bool* called_ptr, const std::u16string&, Browser* browser) {
        auto item = CreateNoOpCommandItem(u"first", 100);
        item->command =
            base::BindOnce([](bool* called) { *called = true; }, called_ptr);
        CommandSource::CommandResults result;
        result.push_back(std::move(item));
        return result;
      },
      &item_called));
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"foobar", browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  CommanderViewModel model = received_view_models_.back();
  controller->OnCommandSelected(1, model.result_set_id);

  EXPECT_FALSE(item_called);
}

TEST_F(CommanderControllerTest, InvokingCompositeCommandSendsPrompt) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  auto source = std::make_unique<TestCommandSource>(
      base::BindRepeating([](const std::u16string&, Browser* browser) {
        auto item = CreateNoOpCommandItem(u"first", 100);
        CommandItem::CompositeCommandProvider noop =
            base::BindRepeating([](const std::u16string&) {
              return CommandSource::CommandResults();
            });
        item->command = std::make_pair(u"Do stuff", noop);
        CommandSource::CommandResults result;
        result.push_back(std::move(item));
        return result;
      }));
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"abracadabra", browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnCommandSelected(0,
                                  received_view_models_.back().result_set_id);
  }
  EXPECT_EQ(received_view_models_.back().action,
            CommanderViewModel::Action::kPrompt);
  EXPECT_EQ(received_view_models_.back().prompt_text, u"Do stuff");
}

TEST_F(CommanderControllerTest, OnTextChangedPassedToCompositeCommandProvider) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  std::u16string received_string;
  auto source = std::make_unique<TestCommandSource>(base::BindRepeating(
      [](std::u16string* passthrough_string, const std::u16string& string,
         Browser* browser) {
        auto item = CreateNoOpCommandItem(u"first", 100);
        CommandItem::CompositeCommandProvider provider = base::BindRepeating(
            [](std::u16string* out_string, const std::u16string& string) {
              *out_string = string;
              return CommandSource::CommandResults();
            },
            passthrough_string);
        item->command = std::make_pair(u"Do stuff", provider);
        CommandSource::CommandResults result;
        result.push_back(std::move(item));
        return result;
      },
      &received_string));
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"abracadabra", browser());
  }
  ASSERT_EQ(received_view_models_.size(), 1u);
  {
    ViewModelCallbackWaiter waiter(this);

    controller->OnCommandSelected(0,
                                  received_view_models_.back().result_set_id);
  }

  controller->OnTextChanged(u"hocus pocus", browser());
  EXPECT_EQ(received_string, u"hocus pocus");
}

TEST_F(CommanderControllerTest,
       CompositeProviderCommandsArePresentedAndExecuted) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  auto source = std::make_unique<TestCommandSource>(
      base::BindRepeating([](const std::u16string&, Browser* browser) {
        auto outer = CreateNoOpCommandItem(u"outer", 100);
        CommandItem::CompositeCommandProvider provider =
            base::BindRepeating([](const std::u16string&) {
              CommandSource::CommandResults results;
              auto inner = CreateNoOpCommandItem(u"inner", 100);
              inner->command = base::MakeExpectedRunClosure(FROM_HERE);
              results.push_back(std::move(inner));
              return results;
            });
        outer->command = std::make_pair(u"Do stuff", provider);
        CommandSource::CommandResults result;
        result.push_back(std::move(outer));
        return result;
      }));
  sources.push_back(std::move(source));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));
  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"abracadabra", browser());
  }

  ASSERT_EQ(received_view_models_.size(), 1u);
  // Select composite command.
  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnCommandSelected(0,
                                  received_view_models_.back().result_set_id);
  }
  // Query again. Controller should pull results from the composite provider
  // this time.
  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"hocus pocus", browser());
  }
  ASSERT_EQ(received_view_models_.size(), 3u);
  EXPECT_EQ(received_view_models_.back().items[0].title, u"inner");

  controller->OnCommandSelected(0, received_view_models_.back().result_set_id);
  // Inner command is an ExpectedRunClosure, so we will fail here if it wasn't
  // called, without needing to assert anything.
}

TEST_F(CommanderControllerTest, OnCompositeCommandCancelledRemovesProvider) {
  std::vector<std::unique_ptr<CommandSource>> sources;
  TestCommandSource* source = AddSource(
      &sources,
      std::make_unique<TestCommandSource>(
          base::BindRepeating([](const std::u16string&, Browser* browser) {
            auto item = CreateNoOpCommandItem(u"first", 100);
            CommandItem::CompositeCommandProvider noop =
                base::BindRepeating([](const std::u16string&) {
                  return CommandSource::CommandResults();
                });
            item->command = std::make_pair(u"Do stuff", noop);
            CommandSource::CommandResults result;
            result.push_back(std::move(item));
            return result;
          })));
  auto controller =
      CommanderController::CreateWithSourcesForTesting(std::move(sources));
  controller->SetUpdateCallback(base::BindRepeating(
      &CommanderControllerTest::OnViewModelUpdated, base::Unretained(this)));

  // Prime the sources so we can select an item.
  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"abracadabra", browser());
  }
  EXPECT_EQ(source->invocations().size(), 1u);

  // Selecting
  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnCommandSelected(0,
                                  received_view_models_.back().result_set_id);
  }
  ASSERT_EQ(received_view_models_.size(), 2u);
  EXPECT_EQ(received_view_models_.back().action,
            CommanderViewModel::Action::kPrompt);
  // This should go to the provider and not be seen by the source.
  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"alakazam", browser());
  }
  EXPECT_EQ(source->invocations().size(), 1u);

  controller->OnCompositeCommandCancelled();
  // Composite command was cancelled, so the source should see this one.
  {
    ViewModelCallbackWaiter waiter(this);
    controller->OnTextChanged(u"hocus pocus", browser());
  }
  EXPECT_EQ(source->invocations().size(), 2u);
  EXPECT_EQ(source->invocations().back(), u"hocus pocus");
}

}  // namespace commander
