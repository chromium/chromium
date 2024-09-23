// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_forest.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/form_forest_test_api.h"
#include "components/autofill/core/browser/form_forest_util_inl.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using FrameData = autofill::internal::FormForest::FrameData;
using FrameDataSet =
    base::flat_set<std::unique_ptr<FrameData>, FrameData::CompareByFrameToken>;

using ::testing::AllOf;
using ::testing::ByRef;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::UnorderedElementsAreArray;

namespace autofill::internal {
namespace {

// Matchers.

auto Equals(const FormFieldData& exp);
auto Equals(const FormData& exp);
auto Equals(const FrameData& exp);

template <typename T>
auto ArrayEquals(const std::vector<T>& exp) {
  std::vector<Matcher<T>> matchers;
  for (const T& f : exp) {
    matchers.push_back(Equals(f));
  }
  return ElementsAreArray(matchers);
}

template <typename T>
auto UnorderedArrayEquals(const std::vector<T>& exp) {
  std::vector<Matcher<T>> matchers;
  for (const T& f : exp) {
    matchers.push_back(Equals(f));
  }
  return UnorderedElementsAreArray(matchers);
}

// The relevant attributes are FormFieldData::global_id(), FormFieldData::value.
// We additionally compare a few more attributes just for safety.
auto Equals(const FormFieldData& exp) {
  return AllOf(
      Property("global_id", &FormFieldData::global_id, exp.global_id()),
      Property("name", &FormFieldData::name, exp.name()),
      Property("host_form_id", &FormFieldData::host_form_id,
               exp.host_form_id()),
      Property("origin", &FormFieldData::origin, exp.origin()),
      Property("form_control_type", &FormFieldData::form_control_type,
               exp.form_control_type()),
      Property("value", &FormFieldData::value, exp.value()),
      Property("label", &FormFieldData::label, exp.label()),
      Property("host_form_signature", &FormFieldData::host_form_signature,
               exp.host_form_signature()));
}

// The relevant attributes are FormData::global_id(), FormData::fields.
// We additionally compare a few more attributes just for safety.
auto Equals(const FormData& exp) {
  return AllOf(
      Property("global_id", &FormData::global_id, Eq(exp.global_id())),
      Property("name", &FormData::name, exp.name()),
      Property("main_frame_origin", &FormData::main_frame_origin,
               exp.main_frame_origin()),
      Property("action", &FormData::action, exp.action()),
      Property("full_url", &FormData::full_url, exp.full_url()),
      Property("url", &FormData::url, exp.url()),
      Property("fields", &FormData::fields, ArrayEquals(exp.fields())));
}

// Compares all attributes of FrameData.
auto Equals(const FrameData& exp) {
  return AllOf(Field("frame_token", &FrameData::frame_token, exp.frame_token),
               Field("child_forms", &FrameData::child_forms,
                     ArrayEquals(exp.child_forms)),
               Field("parent_form", &FrameData::parent_form, exp.parent_form),
               Field("driver", &FrameData::driver, exp.driver));
}

// Deep comparison of the unique_ptrs in a FrameDataSet.
auto Equals(const FrameDataSet& exp) {
  std::vector<Matcher<std::unique_ptr<FrameData>>> matchers;
  for (const std::unique_ptr<FrameData>& x : exp) {
    matchers.push_back(Pointee(Equals(*x)));
  }
  return ElementsAreArray(matchers);
}

// Compares all attributes of FormForest. (Since frame_datas_ is private, we use
// its accessor.)
auto Equals(const FormForest& exp) {
  return Property(&FormForest::frame_datas, Equals(exp.frame_datas()));
}

// Test form.

// The basic test form is a credit card form with six fields: first name, last
// name, number, month, year, CVC.
FormData CreateForm() {
  FormData form = test::CreateTestCreditCardFormData(true, false, true);
  CHECK_EQ(form.fields().size(), 6u);
  return form;
}

// Creates a field type map for the form with N >= 0 repetitions of the fields
// from CreateForm().
auto CreateFieldTypeMap(const FormData& form) {
  CHECK_EQ(form.fields().size() % 6, 0u);
  CHECK_GT(form.fields().size() / 6, 0u);
  base::flat_map<FieldGlobalId, FieldType> map;
  for (size_t i = 0; i < form.fields().size() / 6; ++i) {
    map[form.fields()[6 * i + 0].global_id()] = CREDIT_CARD_NAME_FIRST;
    map[form.fields()[6 * i + 1].global_id()] = CREDIT_CARD_NAME_LAST;
    map[form.fields()[6 * i + 2].global_id()] = CREDIT_CARD_NUMBER;
    map[form.fields()[6 * i + 3].global_id()] = CREDIT_CARD_EXP_MONTH;
    map[form.fields()[6 * i + 4].global_id()] = CREDIT_CARD_EXP_4_DIGIT_YEAR;
    map[form.fields()[6 * i + 5].global_id()] = CREDIT_CARD_VERIFICATION_CODE;
  }
  return map;
}

// A profile is a 6-bit integer, whose bits indicate different values of first
// and last name, credit card number, expiration month, expiration year, CVC.
using Profile = base::StrongAlias<struct ProfileTag, size_t>;
using ::autofill::test::WithoutValues;

// Fills the fields 0..5 of |form| with data according to |profile|, the
// fields 6..11 with |profile|+1, etc.
FormData WithValues(FormData form, Profile profile = Profile(0)) {
  std::vector<FormFieldData>& fields = test_api(form).fields();
  CHECK_EQ(fields.size() % 6, 0u);
  CHECK_GT(fields.size() / 6, 0u);
  for (size_t i = 0; i < fields.size() / 6; ++i) {
    std::bitset<6> bitset(profile.value() + i);
    fields[6 * i + 0].set_value(bitset.test(0) ? u"Jane" : u"John");
    fields[6 * i + 1].set_value(bitset.test(1) ? u"Doe" : u"Average");
    fields[6 * i + 2].set_value(bitset.test(2) ? u"4444333322221111"
                                               : u"4444444444444444");
    fields[6 * i + 3].set_value(bitset.test(3) ? u"01" : u"12");
    fields[6 * i + 4].set_value(bitset.test(4) ? u"2083" : u"2087");
    fields[6 * i + 5].set_value(bitset.test(5) ? u"123" : u"456");
  }
  return form;
}

// Utility functions and constants.

url::Origin Origin(const GURL& url) {
  return url::Origin::Create(url);
}

url::Origin Origin(std::string_view url) {
  return Origin(GURL(url));
}

// Use strings for non-opaque origins and URLs because constructors must not be
// called before the test is set up.
const std::string kMainUrl("https://main.frame.com/");
const std::string kIframeUrl("https://iframe.frame.com/");
const std::string kOtherUrl("https://other.frame.com/");

FrameDataSet& frame_datas(FormForest& ff) {
  return test_api(ff).frame_datas();
}

// Flattens a vector by concatenating the elements of the outer vector.
template <typename T>
std::vector<T> Flattened(const std::vector<std::vector<T>>& xs) {
  std::vector<T> concat;
  for (const auto& x : xs) {
    concat.insert(concat.end(), x.begin(), x.end());
  }
  return concat;
}

// Computes all permutations of |xs|.
// Intended for testing::ValuesIn().
template <typename T>
std::vector<std::vector<T>> Permutations(const std::vector<T>& xs) {
  auto factorial = [](size_t n) -> size_t {
    return std::round(std::tgamma(n + 1));
  };
  std::vector<std::vector<T>> ps;
  ps.reserve(factorial(xs.size()));
  ps.push_back(xs);
  base::ranges::sort(ps.front());
  while (base::ranges::next_permutation(ps.front())) {
    ps.push_back(ps.front());
  }
  CHECK_EQ(ps.size(), factorial(xs.size()));
  return ps;
}

// Computes the permutations of |xs| and concatenates each permutation.
// For example,
//   FlattenedPermutations({{"a", "b"}, {"x", "y"}})
// returns
//   { {"a", "b", "x", "y"},
//     {"x", "y", "a", "b"} }
// because
//   Permutations({{"a", "b"}, {"x", "y"}})
// returns
//   { {{"a", "b"}, {"x", "y"}},
//     {{"x", "y"}, {"a", "b"}} }
// and
//   Flatten({{"a", "b"}, {"x", "y"}})
// returns
//   {"a", "b", "x", "y"}.
// Intended for testing::ValuesIn(), in particular for
// FormForestTestUpdateOrder.
template <typename T>
std::vector<std::vector<T>> FlattenedPermutations(
    const std::vector<std::vector<T>>& xs) {
  return base::ToVector(Permutations(xs), &Flattened<std::string>);
}

// Mimics typical //content layer behaviour:
// - A driver identifies
//    - a same-origin child by its LocalFrameToken and
//    - a cross-origin child by a RemoteFrameToken.
// - A parent driver can inherit, enable, or disable the `shared-autofill`
//   permission in each descendant frames.
class FakeAutofillDriver : public TestAutofillDriver {
 public:
  // `shared-autofill` may be enabled or disabled per driver. This enum mimics
  // the behaviour of HTML policy-controlled features. See
  // SetSharedAutofillByPolicy() for the semantics.
  enum class SharedAutofillPolicy { kDefault, kEnabled, kDisabled };

  static std::unique_ptr<FakeAutofillDriver> CreateChildFrame(
      AutofillClient* client,
      const url::Origin& origin,
      FakeAutofillDriver* parent,
      SharedAutofillPolicy shared_autofill) {
    auto driver = base::WrapUnique(new FakeAutofillDriver(client, origin));
    driver->SetParent(parent);
    driver->SetLocalFrameToken(test::MakeLocalFrameToken());
    if (parent && driver->origin() != parent->origin()) {
      parent->SetRemoteFrameToken(test::MakeRemoteFrameToken(),
                                  driver->GetFrameToken());
    }
    driver->SetSharedAutofillByPolicy(shared_autofill);
    return driver;
  }

  const url::Origin& main_origin() {
    auto* driver = this;
    while (driver->GetParent()) {
      driver = driver->GetParent();
    }
    return driver->origin();
  }

  const url::Origin& origin() const { return origin_; }

  FakeAutofillDriver* GetParent() override {
    return static_cast<FakeAutofillDriver*>(TestAutofillDriver::GetParent());
  }

  FakeAutofillDriver* GetClosestSameOriginAncestor() {
    for (FakeAutofillDriver* ancestor = GetParent(); ancestor;
         ancestor = ancestor->GetParent()) {
      if (origin() == ancestor->origin()) {
        return ancestor;
      }
    }
    return nullptr;
  }

  // Mimics how the policy-controlled feature `shared-autofill` is enabled and
  // disabled in frames.
  void SetSharedAutofillByPolicy(SharedAutofillPolicy shared_autofill) {
    FakeAutofillDriver* ancestor = GetClosestSameOriginAncestor();
    switch (shared_autofill) {
      case SharedAutofillPolicy::kDefault:
        SetSharedAutofill(
            !GetParent() ||
            (ancestor && ancestor->HasSharedAutofillPermission()));
        break;
      case SharedAutofillPolicy::kEnabled:
        CHECK(!GetParent() || GetParent()->HasSharedAutofillPermission())
            << "A parent frame can enable shared-autofill in a subframe only "
               "if shared-autofill is enabled in that parent frame";
        SetSharedAutofill(true);
        break;
      case SharedAutofillPolicy::kDisabled:
        SetSharedAutofill(false);
        break;
    }
  }

  // Mimics ContentAutofillDriver::Lift().
  [[nodiscard]] FormData Lift(FormData form) {
    form.set_host_frame(GetFrameToken());
    form.set_main_frame_origin(main_origin());
    for (FormFieldData& field : test_api(form).fields()) {
      field.set_host_frame(form.host_frame());
      field.set_host_form_id(form.renderer_id());
      field.set_origin(origin());
    }
    return form;
  }

  MOCK_METHOD(void,
              TriggerFormExtractionInDriverFrame,
              (AutofillDriverRouterAndFormForestPassKey),
              (override));

  // Fakes whether a subframe is a root frame from the perspective of
  // MockFlattening(). In the real world, this can happen, for example, because
  // the frame's parent has not been seen yet, or because the frame itself
  // became invisible and hence got cut off by its parent.
  void set_sub_root(bool b) { is_sub_root_ = b; }
  bool is_sub_root() const { return is_sub_root_; }

 private:
  explicit FakeAutofillDriver(AutofillClient* client, const url::Origin& origin)
      : TestAutofillDriver(client), origin_(origin) {}

  const url::Origin origin_;
  bool is_sub_root_ = false;
};

// Fundamental test fixtures.

// Test fixture for all FormForest tests.
//
// Within FormForestTest, we use RemoteFrameTokens only to test scenarios where
// the token cannot be resolved to a LocalFrameToken. In any other case, frames
// shall have LocalFrameTokens. This simplifies the mocking machinery needed
// (that is to say, I couldn't figure out how to mock frames with
// RemoteFrameTokens.)
class FormForestTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// Test fixture with a mocked frame/form tree.
//
// FrameInfo and FormInfo represent mocked forms and can be syntactically
// arranged in a tree structure using designated initializers.
class FormForestTestWithMockedTree : public FormForestTest {
 public:
  struct FormInfo;

  struct FrameInfo {
    // The default value of |url| is changed to kMainUrl or kIframeUrl in
    // MockFormForest().
    std::string url = "";
    std::vector<FormInfo> forms = {};
    FakeAutofillDriver::SharedAutofillPolicy policy =
        FakeAutofillDriver::SharedAutofillPolicy::kDefault;
    // The index of the last field from the parent form that precedes this
    // frame. This is analogous to FormData::child_frames[i].predecessor.
    int field_predecessor = std::numeric_limits<int>::max();
  };

  struct FormInfo {
    std::string name = "";
    FormData form = CreateForm();
    std::vector<FrameInfo> frames = {};
  };

  struct FormSpan {
    std::string form;
    size_t begin = 0;
    size_t count = base::dynamic_extent;
  };

  void TearDown() override {
    test_api(mocked_forms_).Reset();
    test_api(flattened_forms_).Reset();
    drivers_.clear();
    forms_.clear();
    FormForestTest::TearDown();
  }

  // Initializes the |mocked_forms_| according to the frame/form tree
  // |frame_info|.
  FakeAutofillDriver* MockFormForest(
      const FrameInfo& frame_info,
      FakeAutofillDriver* parent_driver = nullptr,
      FormData* parent_form = nullptr) {
    CHECK_EQ(!parent_driver, !parent_form);
    GURL url(!frame_info.url.empty() ? frame_info.url
             : !parent_driver        ? kMainUrl
                                     : kIframeUrl);
    drivers_.push_back(FakeAutofillDriver::CreateChildFrame(
        &client_, Origin(url), /*parent=*/parent_driver,
        /*shared_autofill=*/frame_info.policy));
    FakeAutofillDriver* driver = drivers_.back().get();

    std::vector<FormData> forms;
    for (const FormInfo& form_info : frame_info.forms) {
      FormData data = form_info.form;
      data.set_name(base::ASCIIToUTF16(form_info.name));
      data.set_url(url);
      for (FormFieldData& field : test_api(data).fields()) {
        field.set_name(base::StrCat({data.name(), u".", field.name()}));
      }
      data = driver->Lift(data);

      // Creates the frames and set their predecessor field according to
      // FrameInfo::field_predecessor. By default, the frames come after all
      // fields.
      std::vector<FrameTokenWithPredecessor> child_frames;
      for (const FrameInfo& subframe_info : form_info.frames) {
        FakeAutofillDriver* child =
            MockFormForest(subframe_info, driver, &data);
        child_frames.emplace_back();
        child_frames.back().token = child->GetFrameToken();
        child_frames.back().predecessor =
            std::min(static_cast<int>(data.fields().size()),
                     subframe_info.field_predecessor);
      }
      data.set_child_frames(std::move(child_frames));

      if (!form_info.name.empty()) {
        CHECK(!base::Contains(forms_, form_info.name));
        forms_.emplace(form_info.name, data.global_id());
      }
      forms.push_back(data);
    }

    auto frame_data = std::make_unique<FrameData>(driver->GetFrameToken());
    frame_data->child_forms = std::move(forms);
    if (parent_form) {
      frame_data->parent_form = parent_form->global_id();
    }
    frame_data->driver = driver;
    auto p = frame_datas(mocked_forms_).insert(std::move(frame_data));
    CHECK(p.second);
    return driver;
  }

  using ForceReset = base::StrongAlias<struct ForceFlattenTag, bool>;

  // Mocks flattening of |form_fields| into their root form.
  //
  // Exactly one form mentioned in |form_fields| must be a root form in. The
  // function flattens the fields specified by |form_fields| into this root, in
  // the same order they appear in |form_fields|.
  //
  // MockFlattening() should be called only after MockFormForest(), as its first
  // call copies the forms without their fields from |mocked_forms_| to
  // |flattened_forms_|.
  //
  // To force such a copy in later calls (because the |flattened_forms_| may
  // have changed in the meantime), set |force_flatten| to true.
  void MockFlattening(const std::vector<FormSpan>& form_fields,
                      ForceReset force_flatten = ForceReset(false)) {
    // Collect fields.
    std::vector<FormFieldData> fields;
    for (FormSpan f : form_fields) {
      const FormData& source = GetMockedForm(f.form);
      if (f.begin >= source.fields().size()) {
        continue;
      }
      if (f.begin + f.count > source.fields().size()) {
        f.count = base::dynamic_extent;
      }
      base::ranges::copy(
          base::make_span(source.fields()).subspan(f.begin, f.count),
          std::back_inserter(fields));
    }

    // Copy |mocked_forms_| into |flattened_forms_|, without fields.
    if (frame_datas(flattened_forms_).empty() || force_flatten) {
      test_api(flattened_forms_).Reset();
      std::vector<std::unique_ptr<FrameData>> copy;
      for (const auto& frame : frame_datas(mocked_forms_)) {
        copy.push_back(std::make_unique<FrameData>(frame->frame_token));
        copy.back()->parent_form = frame->parent_form;
        copy.back()->child_forms = frame->child_forms;
        for (FormData& child_form : copy.back()->child_forms) {
          child_form.set_fields({});
        }
        copy.back()->driver = frame->driver;
      }
      frame_datas(flattened_forms_) = FrameDataSet(std::move(copy));
    }

    // Copy fields to the root.
    auto IsRoot = [this](FormSpan fs) {
      FakeAutofillDriver& d = GetDriverOfForm(fs.form);
      return !d.GetParent() || d.is_sub_root();
    };
    auto it = base::ranges::find_if(form_fields, IsRoot);
    CHECK(it != form_fields.end());
    CHECK(std::ranges::all_of(form_fields, [&](FormSpan fs) {
      return !IsRoot(fs) || fs.form == it->form;
    }));
    GetFlattenedForm(it->form).set_fields(fields);

    // Validate flattening.
    CHECK_EQ(frame_datas(flattened_forms_).size(),
             frame_datas(mocked_forms_).size());
    auto IsRoorOrEmpty = [](const auto& frame) {
      return !frame->parent_form ||
             std::ranges::all_of(frame->child_forms,
                                 &std::vector<FormFieldData>::empty,
                                 &FormData::fields);
    };
    CHECK(std::ranges::all_of(frame_datas(flattened_forms_), IsRoorOrEmpty));
  }

  FakeAutofillDriver& GetDriverOfForm(std::string_view form_name) {
    LocalFrameToken frame_token = GetMockedForm(form_name).host_frame();
    const FrameData* frame_data =
        test_api(mocked_forms_).GetFrameData(frame_token);
    CHECK(frame_data) << form_name;
    CHECK(frame_data->driver) << form_name;
    return static_cast<FakeAutofillDriver&>(*frame_data->driver);
  }

  FrameData& GetMockedFrame(std::string_view form_name) {
    FakeAutofillDriver& d = GetDriverOfForm(form_name);
    FrameData* frame = test_api(mocked_forms_).GetFrameData(d.GetFrameToken());
    CHECK(frame) << form_name;
    return *frame;
  }

  FormData& GetMockedForm(std::string_view form_name) {
    auto it = forms_.find(form_name);
    CHECK(it != forms_.end()) << form_name;
    FormData* form = test_api(mocked_forms_).GetFormData(it->second);
    CHECK(form);
    return *form;
  }

  FormData& GetFlattenedForm(std::string_view form_name) {
    CHECK(!GetDriverOfForm(form_name).GetParent() ||
          GetDriverOfForm(form_name).is_sub_root());
    auto it = forms_.find(form_name);
    CHECK(it != forms_.end()) << form_name;
    FormData* form = test_api(flattened_forms_).GetFormData(it->second);
    CHECK(form);
    return *form;
  }

  FormForest mocked_forms_;
  FormForest flattened_forms_;

 private:
  TestAutofillClient client_;
  std::vector<std::unique_ptr<FakeAutofillDriver>> drivers_;
  std::map<std::string, FormGlobalId, std::less<>> forms_;
};

// Tests of FormForest::UpdateTreeOfRendererForm().

class FormForestTestUpdateTree : public FormForestTestWithMockedTree {
 public:
  // The subject of this test fixture.
  void UpdateTreeOfRendererForm(FormForest& ff, std::string_view form_name) {
    ff.UpdateTreeOfRendererForm(GetMockedForm(form_name),
                                GetDriverOfForm(form_name));
  }
};

// Tests that different root forms are not merged.
TEST_F(FormForestTestUpdateTree, MultipleRoots) {
  MockFormForest(
      {.forms = {
           {.name = "main1", .frames = {{.forms = {{.name = "child1"}}}}},
           {.name = "main2", .frames = {{.forms = {{.name = "child2"}}}}}}});
  MockFlattening({{"main1"}, {"child1"}});
  MockFlattening({{"main2"}, {"child2"}});
  FormForest ff;
  UpdateTreeOfRendererForm(ff, "child1");
  UpdateTreeOfRendererForm(ff, "child2");
  UpdateTreeOfRendererForm(ff, "main1");
  UpdateTreeOfRendererForm(ff, "main2");
  EXPECT_THAT(ff, Equals(flattened_forms_));
}

// Tests that (only) for forms with unseen parent form,
// TriggerFormExtractionInDriverFrame() is called on the parent frame.
TEST_F(FormForestTestUpdateTree, TriggerFormExtractionInDriverFrame) {
  MockFormForest(
      {.forms = {
           {.name = "main1", .frames = {{.forms = {{.name = "child1"}}}}},
           {.name = "main2", .frames = {{.forms = {{.name = "child2"}}}}}}});
  MockFlattening({{"main1"}, {"child1"}});
  MockFlattening({{"main2"}, {"child2"}});
  FormForest ff;
  EXPECT_CALL(GetDriverOfForm("main1"), TriggerFormExtractionInDriverFrame)
      .Times(1);
  UpdateTreeOfRendererForm(ff, "child1");
  EXPECT_CALL(GetDriverOfForm("main1"), TriggerFormExtractionInDriverFrame)
      .Times(0);
  UpdateTreeOfRendererForm(ff, "main1");
  EXPECT_CALL(GetDriverOfForm("main1"), TriggerFormExtractionInDriverFrame)
      .Times(0);
  UpdateTreeOfRendererForm(ff, "child1");
  EXPECT_CALL(GetDriverOfForm("main2"), TriggerFormExtractionInDriverFrame)
      .Times(1);
  UpdateTreeOfRendererForm(ff, "child2");
  EXPECT_CALL(GetDriverOfForm("main2"), TriggerFormExtractionInDriverFrame)
      .Times(0);
  UpdateTreeOfRendererForm(ff, "main2");
  EXPECT_THAT(ff, Equals(flattened_forms_));
}

// Tests that at most 64 descendants are flattened into their root.
//
// The test creates a single root form (FormName(0)) with 30 child frames, each
// of which contains 3 forms, so there's a total of 90 forms.
// UpdateTreeOfRendererForm() flattens (only) the first 64 of these descendant
// forms.
TEST_F(FormForestTestUpdateTree, SizeLimit) {
  auto FormName = [](size_t num) -> std::string {
    return std::string("form") + base::NumberToString(num);
  };
  // The number of maximum descendants (= node ranges) according to
  // FormForest::UpdateTreeOfRendererForm()::kMaxVisits.
  constexpr size_t kMaxFlattened = 64;
  // The number of descendants that will actually get flattened. This may be
  // less than kMaxFlattened because UpdateTreeOfRendererForm() either flattens
  // all fields from a frame or none at all.
  constexpr size_t kActualFlattened = kMaxFlattened / 3 * 3;
  // The number of descendants we generate here, some of which will be flattened
  // and some of which will not.
  constexpr size_t kDescendants = 90;
  static_assert(kActualFlattened < kMaxFlattened, "");
  static_assert(kDescendants % 3 == 0, "");

  // Generate the tree with kDescendant child forms in groups of three per
  // frame. Then detach the frames whose forms will not be flattened.
  MockFormForest([&] {
    FrameInfo root{.forms = {{.name = FormName(0)}}};
    for (size_t i = 0; i < kDescendants / 3; ++i) {
      root.forms.front().frames.push_back(
          {.forms = {{.name = FormName(3 * i + 1)},
                     {.name = FormName(3 * i + 2)},
                     {.name = FormName(3 * i + 3)}}});
    }
    return root;
  }());
  for (size_t i = kActualFlattened + 1; i <= kDescendants; ++i) {
    GetDriverOfForm(FormName(i)).set_sub_root(true);
    GetMockedFrame(FormName(i)).parent_form = std::nullopt;
  }

  MockFlattening([&] {
    std::vector<FormSpan> flattened_forms;
    for (size_t i = 0; i <= kActualFlattened; ++i) {
      flattened_forms.push_back({FormName(i)});
    }
    return flattened_forms;
  }());
  for (size_t i = kActualFlattened + 1; i <= kDescendants; ++i) {
    MockFlattening({{FormName(i)}});
  }

  FormForest ff;
  for (size_t i = 0; i <= kDescendants; ++i) {
    UpdateTreeOfRendererForm(ff, FormName(i));
  }
  for (size_t i = kActualFlattened + 1; i <= kDescendants; i += 3) {
    // At the time FormName(64) was seen, its frame contained only this one
    // form, so the overall limit of kMaxDescendants was satisfied. Therefore,
    // its fields were moved to the root and then deleted from the root once
    // another form was seen and the limit was exceeded. We need to see the form
    // again to reinstate its fields.
    //
    // The same holds for field 67 in the next frame, and so on.
    UpdateTreeOfRendererForm(ff, FormName(i + 0));
    // At the time FormName(65) was seen, its frame contained contained two
    // forms, so the overall limit of kMaxDescendants wasn't satisfied anymore.
    // Therefore, its fields weren't moved to the root. However, its fields had
    // been moved to a temporary variable and then lost. We need to see the form
    // again to reinstate its fields.
    //
    // The same holds for field 68 in the next frame, and so on.
    UpdateTreeOfRendererForm(ff, FormName(i + 1));
    // We don't need to see FormName(66) again because already when FormName(65)
    // was seen, the frame's FrameData::parent_form was unset, so FormName(66)
    // was handled as any ordinary (root) form of a (sub)tree.
    //
    // The same holds for field 69 in the next frame, and so on.
  }

  EXPECT_THAT(ff, Equals(flattened_forms_));
}

using FormNameVector = std::vector<std::string>;

// Parameterized by a list of forms, which in this order are added to the
// FormForest.
// Note that among the forms from the same form, the order of calling
// UpdateTreeOfRendererForm() matters.
// Hence, when generating permutations, use FlattenedPermutations() to keep the
// forms from the same frame in stable order.
class FormForestTestUpdateOrder
    : public FormForestTestUpdateTree,
      public ::testing::WithParamInterface<FormNameVector> {
 protected:
  void TearDown() override {
    test_api(ff_).Reset();
    FormForestTestUpdateTree::TearDown();
  }

  void UpdateFormForestAccordingToParamOrder() {
    for (const std::string& form_name : GetParam()) {
      UpdateTreeOfRendererForm(ff_, form_name);
    }
  }

  FormForest ff_;
};

class FormForestTestUpdateVerticalOrder : public FormForestTestUpdateOrder {};

// Tests that children and grandchildren are merged into their root form.
TEST_P(FormForestTestUpdateVerticalOrder, Test) {
  MockFormForest(
      {.forms = {
           {.name = "main",
            .frames = {
                {.url = kIframeUrl,
                 .forms = {{.name = "inner",
                            .frames = {{.forms = {{.name = "leaf"}}}}}}}}}}});
  MockFlattening({{"main"}, {"inner"}, {"leaf"}});
  UpdateFormForestAccordingToParamOrder();
  EXPECT_THAT(ff_, Equals(flattened_forms_));
}

INSTANTIATE_TEST_SUITE_P(FormForestTest,
                         FormForestTestUpdateVerticalOrder,
                         testing::ValuesIn(Permutations(FormNameVector{
                             "main", "inner", "leaf"})));

class FormForestTestUpdateHorizontalMultiFormSingleFrameOrder
    : public FormForestTestUpdateOrder {};

// Tests that siblings from the same frames are merged into their root form.
TEST_P(FormForestTestUpdateHorizontalMultiFormSingleFrameOrder, Test) {
  MockFormForest({.forms = {{.name = "main",
                             .frames = {{.forms = {{.name = "child1"},
                                                   {.name = "child2"}}}}}}});
  MockFlattening({{"main"}, {"child1"}, {"child2"}});
  UpdateFormForestAccordingToParamOrder();
  EXPECT_THAT(ff_, Equals(flattened_forms_));
}

INSTANTIATE_TEST_SUITE_P(
    FormForestTest,
    FormForestTestUpdateHorizontalMultiFormSingleFrameOrder,
    testing::ValuesIn(FlattenedPermutations(
        std::vector<std::vector<std::string>>{{"main"},
                                              {"child1", "child2"}})));

class FormForestTestUpdateHorizontalMultiFrameSingleFormOrder
    : public FormForestTestUpdateOrder {};

// Tests that siblings from different frames are merged into their root form.
TEST_P(FormForestTestUpdateHorizontalMultiFrameSingleFormOrder, Test) {
  MockFormForest({.forms = {{.name = "main",
                             .frames = {{.forms = {{.name = "child1"}}},
                                        {.forms = {{.name = "child2"}}}}}}});
  MockFlattening({{"main"}, {"child1"}, {"child2"}});
  UpdateFormForestAccordingToParamOrder();
  EXPECT_THAT(ff_, Equals(flattened_forms_));
}

INSTANTIATE_TEST_SUITE_P(
    FormForestTest,
    FormForestTestUpdateHorizontalMultiFrameSingleFormOrder,
    testing::ValuesIn(Permutations(FormNameVector{"main", "child1",
                                                  "child2"})));

class FormForestTestUpdateHorizontalMultiFormMultiFrameOrder
    : public FormForestTestUpdateOrder {};

// Tests that siblings from multiple and the same frame are merged into their
// root form.
TEST_P(FormForestTestUpdateHorizontalMultiFormMultiFrameOrder, Test) {
  auto url = [](std::string_view path) {  // Needed due to crbug/1217402.
    return base::StrCat({kMainUrl, path});
  };
  MockFormForest(
      {.url = url("main"),
       .forms = {{.name = "main",
                  .frames = {{.url = url("child1+2"),
                              .forms = {{.name = "child1"}, {.name = "child2"}},
                              .field_predecessor = -1},
                             {.url = url("child3+4"),
                              .forms = {{.name = "child3"}, {.name = "child4"}},
                              .field_predecessor = 5}}}}});
  MockFlattening({{"child1"}, {"child2"}, {"main"}, {"child3"}, {"child4"}});
  UpdateFormForestAccordingToParamOrder();
  EXPECT_THAT(ff_, Equals(flattened_forms_));
}

INSTANTIATE_TEST_SUITE_P(FormForestTest,
                         FormForestTestUpdateHorizontalMultiFormMultiFrameOrder,
                         testing::ValuesIn(FlattenedPermutations(
                             std::vector<std::vector<std::string>>{
                                 {"main"},
                                 {"child1", "child2"},
                                 {"child3", "child4"}})));

using ChildFramePredecessors = std::tuple<int, int, int>;

// Parameterized by the indices of the fields that precede child frames.
class FormForestTestUpdateSplitForm
    : public FormForestTestUpdateTree,
      public ::testing::WithParamInterface<ChildFramePredecessors> {};

// Tests that fields of subforms are inserted into the parent form at the
// index as specified by FormData::child_frame_predecessors.
TEST_P(FormForestTestUpdateSplitForm, Test) {
  int field0 = std::get<0>(GetParam());
  int field1 = std::get<1>(GetParam());
  int field2 = std::get<2>(GetParam());
  ASSERT_LE(-1, field0);
  ASSERT_LE(field0, field1);
  ASSERT_LE(field1, field2);
  MockFormForest(
      {.forms = {{.name = "main",
                  .frames = {{.forms = {{.name = "child1"}, {.name = "child2"}},
                              .field_predecessor = field0},
                             {.forms = {{.name = "child3"}, {.name = "child4"}},
                              .field_predecessor = field1},
                             {.forms = {{.name = "child5"}, {.name = "child6"}},
                              .field_predecessor = field2}}}}});
  MockFlattening({{.form = "main", .count = base::as_unsigned(field0 + 1)},
                  {"child1"},
                  {"child2"},
                  {.form = "main",
                   .begin = base::as_unsigned(field0 + 1),
                   .count = base::as_unsigned(field1 - field0)},
                  {"child3"},
                  {"child4"},
                  {.form = "main",
                   .begin = base::as_unsigned(field1 + 1),
                   .count = base::as_unsigned(field2 - field1)},
                  {"child5"},
                  {"child6"},
                  {.form = "main", .begin = base::as_unsigned(field2 + 1)}});
  FormForest ff;
  UpdateTreeOfRendererForm(ff, "child1");
  UpdateTreeOfRendererForm(ff, "child2");
  UpdateTreeOfRendererForm(ff, "child3");
  UpdateTreeOfRendererForm(ff, "main");
  UpdateTreeOfRendererForm(ff, "child4");
  UpdateTreeOfRendererForm(ff, "child5");
  UpdateTreeOfRendererForm(ff, "child6");
  EXPECT_THAT(ff, Equals(flattened_forms_));
}

INSTANTIATE_TEST_SUITE_P(FormForestTest,
                         FormForestTestUpdateSplitForm,
                         testing::Values(ChildFramePredecessors{-1, -1, -1},
                                         ChildFramePredecessors{-1, 2, 5},
                                         ChildFramePredecessors{1, 1, 1},
                                         ChildFramePredecessors{3, 3, 3},
                                         ChildFramePredecessors{5, 5, 5}));

class FormForestTestUpdateComplexOrder : public FormForestTestUpdateOrder {};

// Tests for a complex tree that all descendants are merged into their root.
TEST_P(FormForestTestUpdateComplexOrder, Test) {
  auto url = [](std::string_view path) {  // Needed due to crbug/1217402.
    return base::StrCat({kMainUrl, path});
  };
  MockFormForest(
      {.url = url("main"),
       .forms = {
           {.name = "main",
            .frames = {
                {.url = url("children"),
                 .forms = {{.name = "child1",
                            .frames = {{.url = url("grandchild1+2"),
                                        .forms = {{.name = "grandchild1"},
                                                  {.name = "grandchild2"}},
                                        .field_predecessor = -1},
                                       {.url = url("grandchild3+4"),
                                        .forms = {{.name = "grandchild3"},
                                                  {.name = "grandchild4"}},
                                        .field_predecessor = 5}}},
                           {.name = "child2"}},
                 .field_predecessor = 2}}}}});
  MockFlattening({{.form = "main", .count = 3},
                  {"grandchild1"},
                  {"grandchild2"},
                  {"child1"},
                  {"grandchild3"},
                  {"grandchild4"},
                  {"child2"},
                  {.form = "main", .begin = 3}});
  UpdateFormForestAccordingToParamOrder();
  EXPECT_THAT(ff_, Equals(flattened_forms_));
}

INSTANTIATE_TEST_SUITE_P(FormForestTest,
                         FormForestTestUpdateComplexOrder,
                         testing::ValuesIn(FlattenedPermutations(
                             std::vector<std::vector<std::string>>{
                                 {"main"},
                                 {"child1", "child2"},
                                 {"grandchild1", "grandchild2"},
                                 {"grandchild3", "grandchild4"}})));

// Tests that erasing a form removes the form and its fields.
TEST_F(FormForestTestUpdateTree, EraseForm_FieldRemoval) {
  MockFormForest(
      {.forms = {
           {.name = "main",
            .frames = {
                {.url = kIframeUrl,
                 .forms = {{.name = "inner",
                            .frames = {{.forms = {{.name = "leaf"}}}}}}}}}}});
  FormForest ff;
  UpdateTreeOfRendererForm(ff, "main");
  UpdateTreeOfRendererForm(ff, "inner");
  UpdateTreeOfRendererForm(ff, "leaf");
  FormGlobalId removed_form = GetMockedForm("leaf").global_id();
  EXPECT_THAT(ff.EraseForms(std::array{removed_form}),
              ElementsAre(GetMockedForm("main").global_id()));
  std::erase_if(
      (*frame_datas(mocked_forms_).find(removed_form.frame_token))->child_forms,
      [&](const FormData& form) { return form.global_id() == removed_form; });
  MockFlattening({{"main"}, {"inner"}});
  ASSERT_EQ(GetFlattenedForm("main").fields().size(), 12u);
  EXPECT_THAT(ff, Equals(flattened_forms_));
}

// Tests that erasing a frame unsets the children's FrameData::parent_form
// pointer.
TEST_F(FormForestTestUpdateTree, EraseForm_ParentReset) {
  MockFormForest(
      {.forms = {
           {.name = "main",
            .frames = {
                {.url = kIframeUrl,
                 .forms = {{.name = "inner",
                            .frames = {{.forms = {{.name = "leaf"}}}}}}}}}}});
  FormForest ff;
  UpdateTreeOfRendererForm(ff, "main");
  UpdateTreeOfRendererForm(ff, "inner");
  UpdateTreeOfRendererForm(ff, "leaf");
  FormGlobalId removed_form = GetMockedForm("inner").global_id();
  EXPECT_THAT(ff.EraseForms(std::array{removed_form}),
              ElementsAre(GetMockedForm("main").global_id()));
  std::erase_if(
      (*frame_datas(mocked_forms_).find(removed_form.frame_token))->child_forms,
      [&](const FormData& form) { return form.global_id() == removed_form; });
  GetDriverOfForm("leaf").set_sub_root(true);
  GetMockedFrame("leaf").parent_form = std::nullopt;
  MockFlattening({{"main"}});
  MockFlattening({{"leaf"}});
  test_api(GetFlattenedForm("main")).Append(GetFlattenedForm("leaf").fields());
  GetFlattenedForm("leaf").set_fields({});
  ASSERT_EQ(GetFlattenedForm("main").fields().size(), 12u);
  ASSERT_EQ(GetFlattenedForm("leaf").fields().size(), 0u);
  EXPECT_THAT(ff, Equals(flattened_forms_));
}

class FormForestTestUpdateEraseFrame
    : public FormForestTestUpdateTree,
      public ::testing::WithParamInterface<bool> {
 public:
  bool keep_frame() const { return GetParam(); }
};

// Tests that erasing a frame removes its form and fields.
TEST_P(FormForestTestUpdateEraseFrame, EraseFrame_FieldRemoval) {
  MockFormForest(
      {.forms = {
           {.name = "main",
            .frames = {
                {.url = kIframeUrl,
                 .forms = {{.name = "inner",
                            .frames = {{.forms = {{.name = "leaf"}}}}}}}}}}});
  FormForest ff;
  UpdateTreeOfRendererForm(ff, "main");
  UpdateTreeOfRendererForm(ff, "inner");
  UpdateTreeOfRendererForm(ff, "leaf");
  ff.EraseFormsOfFrame(GetMockedForm("leaf").host_frame(),
                       /*keep_frame=*/keep_frame());
  if (!keep_frame()) {
    frame_datas(mocked_forms_).erase(GetMockedForm("leaf").host_frame());
  } else {
    (*frame_datas(mocked_forms_).find(GetMockedForm("leaf").host_frame()))
        ->child_forms.clear();
  }
  MockFlattening({{"main"}, {"inner"}});
  ASSERT_EQ(GetFlattenedForm("main").fields().size(), 12u);
  EXPECT_THAT(ff, Equals(flattened_forms_));
}

// Tests that erasing a frame unsets the children's FrameData::parent_form
// pointer.
TEST_P(FormForestTestUpdateEraseFrame, EraseFrame_ParentReset) {
  MockFormForest(
      {.forms = {
           {.name = "main",
            .frames = {
                {.url = kIframeUrl,
                 .forms = {{.name = "inner",
                            .frames = {{.forms = {{.name = "leaf"}}}}}}}}}}});
  FormForest ff;
  UpdateTreeOfRendererForm(ff, "main");
  UpdateTreeOfRendererForm(ff, "inner");
  UpdateTreeOfRendererForm(ff, "leaf");
  ff.EraseFormsOfFrame(GetMockedForm("inner").host_frame(),
                       /*keep_frame=*/keep_frame());
  if (!keep_frame()) {
    frame_datas(mocked_forms_).erase(GetMockedForm("inner").host_frame());
  } else {
    (*frame_datas(mocked_forms_).find(GetMockedForm("inner").host_frame()))
        ->child_forms.clear();
  }
  GetDriverOfForm("leaf").set_sub_root(true);
  GetMockedFrame("leaf").parent_form = std::nullopt;
  MockFlattening({{"main"}});
  MockFlattening({{"leaf"}});
  test_api(GetFlattenedForm("main")).Append(GetFlattenedForm("leaf").fields());
  GetFlattenedForm("leaf").set_fields({});
  ASSERT_EQ(GetFlattenedForm("main").fields().size(), 12u);
  ASSERT_EQ(GetFlattenedForm("leaf").fields().size(), 0u);
  EXPECT_THAT(ff, Equals(flattened_forms_));
}

INSTANTIATE_TEST_SUITE_P(FormForestTest,
                         FormForestTestUpdateEraseFrame,
                         testing::Bool());

// Parameterized with a source and an optional target form and field index.
class FormForestTestUpdateFieldChange : public FormForestTestUpdateTree {
 protected:
  void MockFormForest() {
    auto url = [](std::string_view path) {  // Needed due to crbug/1217402.
      return base::StrCat({kMainUrl, path});
    };
    FormForestTestWithMockedTree::MockFormForest(
        {.url = url("main"),
         .forms = {
             {.name = "main",
              .frames = {{.url = url("child1+2"),
                          .forms = {{.name = "child1"}, {.name = "child2"}},
                          .field_predecessor = -1},
                         {.url = url("child3+4"),
                          .forms = {{.name = "child3"}, {.name = "child4"}},
                          .field_predecessor = 2},
                         {.url = url("child5+6"),
                          .forms = {{.name = "child5"}, {.name = "child6"}},
                          .field_predecessor = 5}}}}});
  }

  void MockFlattening() {
    FormForestTestWithMockedTree::MockFlattening(
        {{"child1"},
         {"child2"},
         {.form = "main", .count = 3},
         {"child3"},
         {"child4"},
         {.form = "main", .begin = 3, .count = 3},
         {"child5"},
         {"child6"},
         {.form = "main", .begin = 6}});
  }

  void UpdateTreeOfAllForms(FormForest& ff) {
    UpdateTreeOfRendererForm(ff, "main");
    UpdateTreeOfRendererForm(ff, "child1");
    UpdateTreeOfRendererForm(ff, "child2");
    UpdateTreeOfRendererForm(ff, "child3");
    UpdateTreeOfRendererForm(ff, "child4");
    UpdateTreeOfRendererForm(ff, "child5");
    UpdateTreeOfRendererForm(ff, "child6");
  }
};

struct FieldSpec {
  std::string form_name;
  size_t field_index;
};

// Removes a field according to the parameter.
class FormForestTestUpdateFieldRemove
    : public FormForestTestUpdateFieldChange,
      public ::testing::WithParamInterface<FieldSpec> {
 protected:
  void DoRemove() {
    FormData& source_form = GetMockedForm(GetParam().form_name);
    size_t source_index = GetParam().field_index;
    test_api(source_form)
        .fields()
        .erase(source_form.fields().begin() + source_index);
  }
};

// Tests that removing fields from a form is reflected in the form tree.
TEST_P(FormForestTestUpdateFieldRemove, Test) {
  MockFormForest();
  MockFlattening();
  FormForest ff;
  UpdateTreeOfAllForms(ff);
  EXPECT_THAT(ff, Equals(flattened_forms_));
  DoRemove();
  MockFlattening();
  UpdateTreeOfRendererForm(ff, GetParam().form_name);
  EXPECT_THAT(ff, Equals(flattened_forms_));
}

INSTANTIATE_TEST_SUITE_P(FormForestTest,
                         FormForestTestUpdateFieldRemove,
                         testing::Values(FieldSpec{"main", 2},
                                         FieldSpec{"main", 3},
                                         FieldSpec{"child1", 0},
                                         FieldSpec{"child2", 1},
                                         FieldSpec{"child3", 2},
                                         FieldSpec{"child4", 3},
                                         FieldSpec{"child5", 4},
                                         FieldSpec{"child6", 5}));

// Adds a new field according to the parameter.
class FormForestTestUpdateFieldAdd
    : public FormForestTestUpdateFieldChange,
      public ::testing::WithParamInterface<FieldSpec> {
 protected:
  void DoAdd() {
    FormData& target_form = GetMockedForm(GetParam().form_name);
    size_t target_index = GetParam().field_index;
    FormFieldData field = target_form.fields().front();
    field.set_name(base::StrCat({field.name(), u"_copy"}));
    field.set_renderer_id(test::MakeFieldRendererId());
    test_api(target_form)
        .fields()
        .insert(target_form.fields().begin() + target_index, field);
  }
};

// Tests that adding a field to forms is reflected in the form tree.
TEST_P(FormForestTestUpdateFieldAdd, Test) {
  MockFormForest();
  MockFlattening();
  FormForest ff;
  UpdateTreeOfAllForms(ff);
  EXPECT_THAT(ff, Equals(flattened_forms_));
  DoAdd();
  MockFlattening();
  UpdateTreeOfRendererForm(ff, GetParam().form_name);
  EXPECT_THAT(ff, Equals(flattened_forms_));
}

INSTANTIATE_TEST_SUITE_P(FormForestTest,
                         FormForestTestUpdateFieldAdd,
                         testing::Values(FieldSpec{"main", 2},
                                         FieldSpec{"main", 3},
                                         FieldSpec{"child1", 0},
                                         FieldSpec{"child2", 1},
                                         FieldSpec{"child3", 2},
                                         FieldSpec{"child4", 3},
                                         FieldSpec{"child5", 4},
                                         FieldSpec{"child6", 5}));

struct FieldMoveSpec {
  FieldSpec source;
  FieldSpec target;
};

// Moves a field from one form to another according to the parameter.
class FormForestTestUpdateFieldMove
    : public FormForestTestUpdateFieldChange,
      public ::testing::WithParamInterface<FieldMoveSpec> {
 protected:
  void DoMove() {
    const FieldMoveSpec& p = GetParam();
    FormData& source_form = GetMockedForm(p.source.form_name);
    size_t source_index = p.source.field_index;
    FormData& target_form = GetMockedForm(p.target.form_name);
    size_t target_index = p.target.field_index;

    FormFieldData field = source_form.fields()[source_index];
    field.set_host_form_id(target_form.renderer_id());

    if (source_index > target_index) {
      test_api(source_form)
          .fields()
          .erase(source_form.fields().begin() + source_index);
      test_api(target_form)
          .fields()
          .insert(target_form.fields().begin() + target_index, field);
    } else {
      test_api(target_form)
          .fields()
          .insert(target_form.fields().begin() + target_index, field);
      test_api(source_form)
          .fields()
          .erase(source_form.fields().begin() + source_index);
    }
  }
};

// Tests that moving fields between forms (of the same frame) is reflected in
// the form tree.
TEST_P(FormForestTestUpdateFieldMove, Test) {
  MockFormForest();
  MockFlattening();
  FormForest ff;
  UpdateTreeOfAllForms(ff);
  EXPECT_THAT(ff, Equals(flattened_forms_));
  DoMove();
  MockFlattening();
  UpdateTreeOfRendererForm(ff, GetParam().source.form_name);
  if (GetParam().source.form_name != GetParam().target.form_name) {
    UpdateTreeOfRendererForm(ff, GetParam().target.form_name);
  }
  EXPECT_THAT(ff, Equals(flattened_forms_));
}

INSTANTIATE_TEST_SUITE_P(
    FormForestTest,
    FormForestTestUpdateFieldMove,
    testing::Values(FieldMoveSpec{{"main", 0}, {"main", 5}},
                    FieldMoveSpec{{"main", 5}, {"main", 0}},
                    FieldMoveSpec{{"child1", 0}, {"child1", 5}},
                    FieldMoveSpec{{"child1", 5}, {"child1", 0}},
                    FieldMoveSpec{{"child1", 1}, {"child1", 4}},
                    FieldMoveSpec{{"child1", 3}, {"child2", 3}},
                    FieldMoveSpec{{"child3", 5}, {"child4", 0}},
                    FieldMoveSpec{{"child6", 5}, {"child5", 0}}));

// Tests that UpdateTreeOfRendererForm() converges, that is, multiple calls are
// no-ops.
TEST_F(FormForestTestUpdateTree, Converge) {
  auto url = [](std::string_view path) {  // Needed due to crbug/1217402.
    return base::StrCat({kMainUrl, path});
  };
  MockFormForest(
      {.url = url("main"),
       .forms = {
           {.name = "main",
            .frames = {
                {.url = url("children"),
                 .forms = {{.name = "child1",
                            .frames = {{.url = url("grandchild1"),
                                        .forms = {{.name = "grandchild1"}}},
                                       {.url = url("grandchild2"),
                                        .forms = {{.name = "grandchild2"}}}}},
                           {.name = "child2"}}}}}}});
  MockFlattening(
      {{"main"}, {"child1"}, {"grandchild1"}, {"grandchild2"}, {"child2"}});

  FormForest ff;
  UpdateTreeOfRendererForm(ff, "main");
  UpdateTreeOfRendererForm(ff, "child1");
  UpdateTreeOfRendererForm(ff, "child2");
  UpdateTreeOfRendererForm(ff, "grandchild1");
  UpdateTreeOfRendererForm(ff, "grandchild2");
  EXPECT_THAT(ff, Equals(flattened_forms_));

  UpdateTreeOfRendererForm(ff, "main");
  UpdateTreeOfRendererForm(ff, "child1");
  UpdateTreeOfRendererForm(ff, "child1");
  UpdateTreeOfRendererForm(ff, "child2");
  UpdateTreeOfRendererForm(ff, "child2");
  UpdateTreeOfRendererForm(ff, "main");
  UpdateTreeOfRendererForm(ff, "grandchild1");
  UpdateTreeOfRendererForm(ff, "grandchild2");
  UpdateTreeOfRendererForm(ff, "grandchild2");
  UpdateTreeOfRendererForm(ff, "grandchild1");
  UpdateTreeOfRendererForm(ff, "child2");
  UpdateTreeOfRendererForm(ff, "child1");
  UpdateTreeOfRendererForm(ff, "main");
  EXPECT_THAT(ff, Equals(flattened_forms_));
}

// Tests that removing a frame from FormData::child_frames removes the fields
// (but not the FrameData; this is taken care of by EraseFormsOfFrame()).
TEST_F(FormForestTestUpdateTree, RemoveFrame) {
  auto url = [](std::string_view path) {  // Needed due to crbug/1217402.
    return base::StrCat({kMainUrl, path});
  };
  // |child1| is a separate variable for better code formatting.
  FormInfo child1 = {
      .name = "child1",
      .frames = {
          {.url = url("grandchild1"), .forms = {{.name = "grandchild1"}}},
          {.url = url("grandchild2"),
           .forms = {{.name = "grandchild2",
                      .frames = {{.url = url("greatgrandchild"),
                                  .forms = {{.name = "greatgrandchild"}}}}}}}}};
  MockFormForest(
      {.url = url("main"),
       .forms = {{.name = "main",
                  .frames = {{.url = url("children"),
                              .forms = {child1, {.name = "child2"}}}}}}});
  MockFlattening({{"main"},
                  {"child1"},
                  {"grandchild1"},
                  {"grandchild2"},
                  {"greatgrandchild"},
                  {"child2"}});

  FormForest ff;
  UpdateTreeOfRendererForm(ff, "main");
  UpdateTreeOfRendererForm(ff, "child1");
  UpdateTreeOfRendererForm(ff, "child2");
  UpdateTreeOfRendererForm(ff, "grandchild1");
  UpdateTreeOfRendererForm(ff, "grandchild2");
  UpdateTreeOfRendererForm(ff, "greatgrandchild");
  EXPECT_THAT(ff, Equals(flattened_forms_));
  ASSERT_EQ(GetFlattenedForm("main").fields().size(), 6u * 6u);

  // Remove the last frame of "child1", which contains "grandchild2" and
  // indirectly "greatgrandchild".
  GetDriverOfForm("grandchild2").set_sub_root(true);
  GetMockedForm("child1").set_child_frames([&] {
    std::vector<FrameTokenWithPredecessor> child_frames =
        GetMockedForm("child1").child_frames();
    child_frames.pop_back();
    return child_frames;
  }());
  GetMockedFrame("grandchild2").parent_form = std::nullopt;
  GetMockedForm("grandchild2").set_fields({});
  GetMockedForm("greatgrandchild").set_fields({});
  MockFlattening({{"main"}, {"child1"}, {"grandchild1"}, {"child2"}},
                 ForceReset(true));
  MockFlattening({{"grandchild2"}, {"greatgrandchild"}});
  ASSERT_EQ(GetFlattenedForm("main").fields().size(), 4u * 6u);
  ASSERT_EQ(GetFlattenedForm("grandchild2").fields().size(), 0u);

  UpdateTreeOfRendererForm(ff, "child1");

  EXPECT_THAT(ff, Equals(flattened_forms_));
}

// Tests of FormForest::GetBrowserForm().

class FormForestTestFlatten : public FormForestTestWithMockedTree {
 protected:
  // The subject of this test fixture.
  FormData GetBrowserForm(std::string_view form_name) {
    return flattened_forms_.GetBrowserForm(
        GetMockedForm(form_name).global_id());
  }
};

// Tests that flattening a single frame is the identity.
TEST_F(FormForestTestFlatten, SingleFrame) {
  MockFormForest({.url = kMainUrl, .forms = {{.name = "main"}}});
  MockFlattening({{"main"}});
  EXPECT_THAT(GetBrowserForm("main"), Equals(GetFlattenedForm("main")));
}

class FormForestTestFlattenHierarchy
    : public FormForestTestFlatten,
      public ::testing::WithParamInterface<std::string> {};

// Tests that a non-trivial tree is flattened into the root.
TEST_P(FormForestTestFlattenHierarchy, TwoFrames) {
  MockFormForest(
      {.forms = {
           {.name = "main",
            .frames = {{.forms = {{.name = "child1"}, {.name = "child2"}}}}},
           {.name = "main2",
            .frames = {{.forms = {{.name = "child3"}, {.name = "child4"}}}}}}});
  MockFlattening({{"main"}, {"child1"}, {"child2"}});
  MockFlattening({{"main2"}, {"child3"}, {"child4"}});
  EXPECT_THAT(GetBrowserForm(GetParam()), Equals(GetFlattenedForm("main")));
}

INSTANTIATE_TEST_SUITE_P(FormForestTest,
                         FormForestTestFlattenHierarchy,
                         testing::Values("main", "child1", "child2"));

// Tests of FormForest::GetRendererFormsOfBrowserFields().

class FormForestTestUnflatten : public FormForestTestWithMockedTree {
 protected:
  // The subject of this test fixture.
  std::vector<FormData> GetRendererFormsOfBrowserFields(
      std::string_view form_name,
      const FormForest::SecurityOptions& security) {
    return flattened_forms_
        .GetRendererFormsOfBrowserFields(
            WithValues(GetFlattenedForm(form_name)).fields(), security)
        .renderer_forms;
  }

  // This shorthand for GetRendererFormsOfBrowserFields() allows passing
  // prvalues for `triggered_origin`, e.g. `Origin("...")`.
  std::vector<FormData> GetRendererFormsOfBrowserFields(
      std::string_view form_name,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, FieldType>& field_type_map) {
    return GetRendererFormsOfBrowserFields(
        form_name,
        FormForest::SecurityOptions{&GetDriverOfForm(form_name).main_origin(),
                                    &triggered_origin, &field_type_map});
  }

  auto FieldTypeMap(std::string_view form_name) {
    return CreateFieldTypeMap(WithValues(GetFlattenedForm(form_name)));
  }
};

// Test that solitaire main frame forms are filled as usual.
TEST_F(FormForestTestUnflatten, MainFrame) {
  MockFormForest({.url = kMainUrl,
                  .forms = {{.name = "main", .frames = {}},
                            {.name = "main2", .frames = {}}}});
  MockFlattening({{"main"}});
  MockFlattening({{"main2"}});
  std::vector<FormData> expectation = {WithValues(GetMockedForm("main"))};
  EXPECT_THAT(GetRendererFormsOfBrowserFields("main", Origin(kMainUrl), {}),
              UnorderedArrayEquals(expectation));
}

// Test that child frame forms are filled as usual.
TEST_F(FormForestTestUnflatten, ChildFrame) {
  MockFormForest({.url = kMainUrl,
                  .forms = {{.name = "main",
                             .frames = {{.url = kIframeUrl,
                                         .forms = {{.name = "child"}}}}}}});
  MockFlattening({{"main"}, {"child"}});
  std::vector<FormData> expectation = {
      GetMockedForm("main"), WithValues(GetMockedForm("child"), Profile(1))};
  EXPECT_THAT(GetRendererFormsOfBrowserFields("main", Origin(kIframeUrl), {}),
              UnorderedArrayEquals(expectation));
}

// Test that a tree of forms is filled (assuming same origins), but other
// neighboring trees are not.
TEST_F(FormForestTestUnflatten, LargeTree) {
  auto url = [](std::string_view path) {  // Needed due to crbug/1217402.
    return base::StrCat({kMainUrl, path});
  };
  MockFormForest(
      {.url = url("main"),
       .forms =
           {{.name = "main",
             .frames = {{.url = url("children"),
                         .forms =
                             {{.name = "child1",
                               .frames = {{.url = url("grandchild1+2"),
                                           .forms = {{.name = "grandchild1"},
                                                     {.name = "grandchild2"}}},
                                          {.url = url("grandchild3+4"),
                                           .forms = {{.name = "grandchild3"},
                                                     {.name =
                                                          "grandchild4"}}}}},
                              {.name = "child2"}}}}},
            {.name = "main2",
             .frames = {
                 {.url = url("nieces"),
                  .forms = {{.name = "niece1"}, {.name = "niece2"}}}}}}});
  MockFlattening({{"main"},
                  {"grandchild1"},
                  {"grandchild2"},
                  {"child1"},
                  {"grandchild3"},
                  {"grandchild4"},
                  {"child2"}});
  MockFlattening({{"main2"}, {"niece1"}, {"niece2"}});
  std::vector<FormData> expectation = {
      WithValues(GetMockedForm("main"), Profile(0)),
      WithValues(GetMockedForm("grandchild1"), Profile(1)),
      WithValues(GetMockedForm("grandchild2"), Profile(2)),
      WithValues(GetMockedForm("child1"), Profile(3)),
      WithValues(GetMockedForm("grandchild3"), Profile(4)),
      WithValues(GetMockedForm("grandchild4"), Profile(5)),
      WithValues(GetMockedForm("child2"), Profile(6))};
  EXPECT_THAT(GetRendererFormsOfBrowserFields("main", Origin(kMainUrl), {}),
              UnorderedArrayEquals(expectation));
}

// Tests that (only) frames from the same origin are filled.
TEST_F(FormForestTestUnflatten, SameOriginPolicy) {
  MockFormForest(
      {.url = kMainUrl,
       .forms = {
           {.name = "main",
            .frames = {{.url = kOtherUrl, .forms = {{.name = "child1"}}},
                       {.url = kIframeUrl, .forms = {{.name = "child2"}}}}}}});
  MockFlattening({{"main"}, {"child1"}, {"child2"}});
  std::vector<FormData> expectation = {
      WithoutValues(GetMockedForm("main")),
      WithoutValues(GetMockedForm("child1")),
      WithValues(GetMockedForm("child2"), Profile(2))};
  EXPECT_THAT(GetRendererFormsOfBrowserFields("main", Origin(kIframeUrl), {}),
              UnorderedArrayEquals(expectation));
}

// Tests that (only) frames from the same origin are filled.
TEST_F(FormForestTestUnflatten, SameOriginPolicyNoValuesErased) {
  MockFormForest(
      {.url = kMainUrl,
       .forms = {
           {.name = "main",
            .frames = {{.url = kOtherUrl, .forms = {{.name = "child1"}}},
                       {.url = kIframeUrl, .forms = {{.name = "child2"}}}}}}});
  MockFlattening({{"main"}, {"child1"}, {"child2"}});
  std::vector<FormData> expectation = {
      WithValues(GetMockedForm("main"), Profile(0)),
      WithValues(GetMockedForm("child1"), Profile(1)),
      WithValues(GetMockedForm("child2"), Profile(2))};
  EXPECT_THAT(GetRendererFormsOfBrowserFields(
                  "main", FormForest::SecurityOptions::TrustAllOrigins()),
              UnorderedArrayEquals(expectation));
}

// Tests that even if a different-origin frame interrupts two same-origin
// frames, they are filled together.
TEST_F(FormForestTestUnflatten, InterruptedSameOriginPolicy) {
  MockFormForest(
      {.url = kMainUrl,
       .forms = {
           {.name = "main",
            .frames = {
                {.url = kIframeUrl,
                 .forms = {{.name = "inner",
                            .frames = {{.url = kMainUrl,
                                        .forms = {{.name = "leaf"}}}}}}}}}}});
  MockFlattening({{"main"}, {"inner"}, {"leaf"}});
  std::vector<FormData> expectation = {
      WithValues(GetMockedForm("main"), Profile(0)),
      WithoutValues(GetMockedForm("inner")),
      WithValues(GetMockedForm("leaf"), Profile(2))};
  EXPECT_THAT(GetRendererFormsOfBrowserFields("main", Origin(kMainUrl), {}),
              UnorderedArrayEquals(expectation));
}

// Tests that (only) non-sensitive fields are filled across origin into the main
// frame's origin (since the main frame has the shared-autofill policy by
// default).
TEST_F(FormForestTestUnflatten, MainOriginPolicy) {
  MockFormForest(
      {.url = kMainUrl,
       .forms = {
           {.name = "main",
            .frames = {{.url = kMainUrl, .forms = {{.name = "child1"}}},
                       {.url = kIframeUrl, .forms = {{.name = "child2"}}}}}}});
  MockFlattening({{"main"}, {"child1"}, {"child2"}});
  std::vector<FormData> expectation = {
      WithValues(GetMockedForm("main"), Profile(0)),
      WithValues(GetMockedForm("child1"), Profile(1)),
      WithValues(GetMockedForm("child2"), Profile(2))};
  // Clear sensitive fields: the credit card number (field index 2) and CVC
  // (field index 5) in the two main-origin forms.
  test_api(expectation[0]).field(2).set_value({});
  test_api(expectation[0]).field(5).set_value({});
  test_api(expectation[1]).field(2).set_value({});
  test_api(expectation[1]).field(5).set_value({});
  EXPECT_THAT(GetRendererFormsOfBrowserFields("main", Origin(kIframeUrl),
                                              FieldTypeMap("main")),
              UnorderedArrayEquals(expectation));
}

// Tests that no fields are filled across origin into frames where
// shared-autofill is disabled (not even into non-sensitive fields).
TEST_F(FormForestTestUnflatten, MainOriginPolicyWithoutSharedAutofill) {
  MockFormForest(
      {.url = kMainUrl,
       .forms = {{.name = "main",
                  .frames = {{.url = kMainUrl, .forms = {{.name = "child1"}}},
                             {.url = kIframeUrl,
                              .forms = {{.name = "child2"}}}}}},
       .policy = FakeAutofillDriver::SharedAutofillPolicy::kDisabled});
  MockFlattening({{"main"}, {"child1"}, {"child2"}});
  std::vector<FormData> expectation = {
      WithoutValues(GetMockedForm("main")),
      WithoutValues(GetMockedForm("child1")),
      WithValues(GetMockedForm("child2"), Profile(2))};
  EXPECT_THAT(GetRendererFormsOfBrowserFields("main", Origin(kIframeUrl),
                                              FieldTypeMap("main")),
              UnorderedArrayEquals(expectation));
}

// Fixture for the shared-autofill policy tests.
class FormForestTestUnflattenSharedAutofillPolicy
    : public FormForestTestUnflatten {
 public:
  void SetUp() override {
    FormForestTestUnflatten::SetUp();
    MockFormForest(
        {.url = kMainUrl,
         .forms = {
             {.name = "main",
              .frames = {
                  {.url = kOtherUrl, .forms = {{.name = "disallowed"}}},
                  {.url = kIframeUrl,
                   .forms = {{.name = "allowed"}},
                   .policy =
                       FakeAutofillDriver::SharedAutofillPolicy::kEnabled}}}}});
    ASSERT_NE(Origin("main"), Origin("allowed"));
    ASSERT_NE(Origin("disallowed"), Origin("allowed"));
  }
};

// Tests filling into frames with shared-autofill policy from the main origin.
TEST_F(FormForestTestUnflattenSharedAutofillPolicy, FromMainOrigin) {
  MockFlattening({{"main"}, {"disallowed"}, {"allowed"}});
  std::vector<FormData> expectation = {
      WithValues(GetMockedForm("main"), Profile(0)),
      WithoutValues(GetMockedForm("disallowed")),
      WithValues(GetMockedForm("allowed"), Profile(2))};
  EXPECT_THAT(GetRendererFormsOfBrowserFields("main", Origin(kMainUrl), {}),
              UnorderedArrayEquals(expectation));
}

// Tests filling into frames with shared-autofill policy from the main origin.
TEST_F(FormForestTestUnflattenSharedAutofillPolicy, FromOtherOrigin) {
  MockFlattening({{"main"}, {"disallowed"}, {"allowed"}});
  std::vector<FormData> expectation = {
      WithoutValues(GetMockedForm("main")),
      WithValues(GetMockedForm("disallowed"), Profile(1)),
      WithoutValues(GetMockedForm("allowed"))};
  EXPECT_THAT(GetRendererFormsOfBrowserFields("main", Origin(kOtherUrl), {}),
              UnorderedArrayEquals(expectation));
}

// Tests irreflexivity, asymmetry, transitivity of FrameData less-than relation.
TEST_F(FormForestTest, FrameDataComparator) {
  FrameData::CompareByFrameToken less;
  std::unique_ptr<FrameData> null;
  auto x = std::make_unique<FrameData>(test::MakeLocalFrameToken());
  auto xx = std::make_unique<FrameData>(x->frame_token);
  auto y = std::make_unique<FrameData>(test::MakeLocalFrameToken());
  ASSERT_TRUE(x->frame_token < y->frame_token);
  EXPECT_FALSE(less(null, null));
  EXPECT_TRUE(less(null, x));
  EXPECT_FALSE(less(x, null));
  EXPECT_FALSE(less(x, x));
  EXPECT_FALSE(less(xx, xx));
  EXPECT_FALSE(less(x, xx));
  EXPECT_FALSE(less(xx, x));
  EXPECT_TRUE(less(x, y));
  EXPECT_FALSE(less(y, x));
}

// Tests of utility functions.

struct ForEachInSetDifferenceTestParam {
  std::vector<size_t> lhs;
  std::vector<size_t> rhs;
  std::vector<size_t> diff;
  size_t expected_comparisons;
};

class ForEachInSetDifferenceTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<ForEachInSetDifferenceTestParam> {
 public:
  // A wrapper of a size_t that counts its calls to operator==().
  class Dummy {
   public:
    size_t val = 0;
    raw_ptr<size_t> num_equals_calls = nullptr;
  };

  std::vector<Dummy> ToDummies(const std::vector<size_t>& vec) {
    std::vector<Dummy> out;
    for (const size_t v : vec) {
      out.push_back({.val = v, .num_equals_calls = &num_equals_calls_});
    }
    return out;
  }

  size_t num_equals_calls_ = 0;
};

bool operator==(ForEachInSetDifferenceTest::Dummy x,
                ForEachInSetDifferenceTest::Dummy y) {
  CHECK(x.num_equals_calls && x.num_equals_calls == y.num_equals_calls);
  ++*x.num_equals_calls;
  return x.val == y.val;
}

// Tests that for_each_in_set_difference() calls the callback for the expected
// elements and checks its number of comparisons.
TEST_P(ForEachInSetDifferenceTest, Test) {
  std::vector<size_t> diff;
  for_each_in_set_difference(ToDummies(GetParam().lhs),
                             ToDummies(GetParam().rhs),
                             [&diff](Dummy d) { diff.push_back(d.val); });
  EXPECT_THAT(diff, ElementsAreArray(GetParam().diff));
  EXPECT_EQ(num_equals_calls_, GetParam().expected_comparisons);
}

INSTANTIATE_TEST_SUITE_P(
    FormForestTest,
    ForEachInSetDifferenceTest,
    testing::Values(
        ForEachInSetDifferenceTestParam{{}, {}, {}, 0},
        ForEachInSetDifferenceTestParam{{}, {1, 2, 3}, {}, 0},
        ForEachInSetDifferenceTestParam{{1}, {1, 2, 3, 4}, {}, 1},
        ForEachInSetDifferenceTestParam{{1, 2, 3}, {1, 2, 3}, {}, 3},
        ForEachInSetDifferenceTestParam{{1, 2, 3}, {1, 2, 3, 4, 5}, {}, 3},
        ForEachInSetDifferenceTestParam{{3, 4, 1, 2}, {1, 2, 3, 4}, {}, 6},
        ForEachInSetDifferenceTestParam{{1, 2, 3, 4}, {1, 2, 3}, {4}, 6},
        ForEachInSetDifferenceTestParam{{1, 2, 3, 4}, {1, 3, 4}, {2}, 6},
        ForEachInSetDifferenceTestParam{{1, 2, 3, 4}, {4, 3, 2, 1}, {}, 13},
        ForEachInSetDifferenceTestParam{{3, 4, 1, 2}, {1, 2, 3}, {4}, 8},
        ForEachInSetDifferenceTestParam{{1, 2, 3, 4}, {1}, {2, 3, 4}, 4},
        ForEachInSetDifferenceTestParam{{1, 2, 3, 4}, {}, {1, 2, 3, 4}, 0}));

}  // namespace
}  // namespace autofill::internal
