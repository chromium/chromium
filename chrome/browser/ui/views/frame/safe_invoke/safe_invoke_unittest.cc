// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/safe_invoke/safe_invoke.h"

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// -----------------------------------------------------------------------------
// Test Fixtures & Helpers
// -----------------------------------------------------------------------------

struct MoveOnlyData {
  std::unique_ptr<int> value;
};

struct OverloadedAddressOf {
  int value = 42;
  void* operator&() const = delete;  // NOLINT(runtime/operator)
};

struct CustomStatusWithHasValue {
  int code = 0;
  bool has_value() const { return code != 0; }
};

class BaseView {
 public:
  bool visible = true;

  void SetVisible(bool v) { visible = v; }
  bool IsVisible() const { return visible; }
};

class DerivedView : public BaseView {
 public:
  std::string text = "hello";
};

class Leaf {
 public:
  int value = 0;

  void Increment() { value++; }
  void Add(int delta) { value += delta; }
  int GetValue() const { return value; }
  std::optional<int> GetOptionalValue(bool return_val) const {
    if (return_val) {
      return value;
    }
    return std::nullopt;
  }

  // Overloaded on const-qualification:
  int Compute(int x) { return value + x; }
  int Compute(int x) const { return value + (x * 2); }

  // Overloaded on arity / parameter count:
  int Process() { return value; }
  int Process(int delta) { return value + delta; }
  int Process(int delta, int mult) { return (value + delta) * mult; }
};

class RefLeaf : public base::RefCounted<RefLeaf> {
 public:
  int value = 0;
  explicit RefLeaf(int v) : value(v) {}
  int GetValue() const { return value; }

 private:
  friend class base::RefCounted<RefLeaf>;
  ~RefLeaf() = default;
};

class WeakLeaf {
 public:
  int value = 0;

  void Add(int delta) { value += delta; }
  int GetValue() const { return value; }
  base::WeakPtr<WeakLeaf> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }
  void InvalidateWeakPtrs() { weak_factory_.InvalidateWeakPtrs(); }

 private:
  base::WeakPtrFactory<WeakLeaf> weak_factory_{this};
};

class Branch {
 public:
  std::unique_ptr<Leaf> leaf = std::make_unique<Leaf>();
  std::unique_ptr<WeakLeaf> weak_leaf = std::make_unique<WeakLeaf>();
  scoped_refptr<RefLeaf> ref_leaf = base::MakeRefCounted<RefLeaf>(100);
  MoveOnlyData move_only;
  OverloadedAddressOf overloaded_addr;
  int branch_id = 10;

  Leaf* GetLeaf() { return leaf.get(); }
  Leaf* GetLeafIf(bool condition) { return condition ? leaf.get() : nullptr; }
  const Leaf* GetConstLeaf() const { return leaf.get(); }

  base::WeakPtr<WeakLeaf> GetWeakLeaf() {
    return weak_leaf ? weak_leaf->AsWeakPtr() : nullptr;
  }
  base::WeakPtr<WeakLeaf> GetNullWeakLeaf() { return nullptr; }
  base::WeakPtr<WeakLeaf> stored_weak_leaf;
  base::WeakPtr<WeakLeaf> GetStoredWeakLeaf() { return stored_weak_leaf; }

  scoped_refptr<RefLeaf> GetRefLeaf() { return ref_leaf; }
  const scoped_refptr<RefLeaf>& GetConstRefLeaf() const { return ref_leaf; }
  scoped_refptr<RefLeaf> GetNullRefLeaf() { return nullptr; }

  MoveOnlyData& GetMoveOnly() { return move_only; }
  OverloadedAddressOf& GetOverloadedAddr() { return overloaded_addr; }

  int GetBranchId() const { return branch_id; }
  int MultiplyBranchId(int factor) const { return branch_id * factor; }
  void ResetLeaf() { leaf->value = 0; }
};

class Tree {
 public:
  std::unique_ptr<Branch> branch = std::make_unique<Branch>();
  Branch branch_member;

  Branch* GetBranch() { return branch.get(); }
  Branch* GetNullBranch() { return nullptr; }
  Branch& GetBranchRef() { return branch_member; }
};

// Free function helpers:
Leaf* FreeGetLeaf(Branch* b) {
  return b ? b->GetLeaf() : nullptr;
}

int FreeComputeScore(Leaf* l, int multiplier) {
  return l ? l->value * multiplier : 0;
}

int FreeCompute(Branch* b) {
  return b ? b->branch_id + 10 : 0;
}

int FreeCompute(Branch* b, int offset) {
  return b ? b->branch_id + offset : 0;
}

// Functor helper:
struct LeafDoubler {
  void operator()(Leaf* l) const {
    if (l) {
      l->value *= 2;
    }
  }
};

// -----------------------------------------------------------------------------
// Basic Invocations & Arguments
// -----------------------------------------------------------------------------

TEST(SafeInvokeUnitTest, WithoutArgsVoidReturn) {
  Leaf leaf;
  leaf.value = 10;

  SafeInvoke(&leaf).Then(&Leaf::Increment);
  EXPECT_EQ(leaf.value, 11);

  Leaf* null_leaf = nullptr;
  SafeInvoke(null_leaf).Then(&Leaf::Increment);
}

TEST(SafeInvokeUnitTest, WithoutArgsPointerReturn) {
  Branch branch;
  branch.leaf->value = 42;

  Leaf* leaf = SafeInvoke(&branch).Then(&Branch::GetLeaf).get();
  ASSERT_NE(leaf, nullptr);
  EXPECT_EQ(leaf->value, 42);

  Branch* null_branch = nullptr;
  Leaf* null_leaf = SafeInvoke(null_branch).Then(&Branch::GetLeaf).get();
  EXPECT_EQ(null_leaf, nullptr);
}

TEST(SafeInvokeUnitTest, WithoutArgsValueReturn) {
  Leaf leaf;
  leaf.value = 99;

  std::optional<int> val = SafeInvoke(&leaf).Then(&Leaf::GetValue);
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(val.value(), 99);

  Leaf* null_leaf = nullptr;
  std::optional<int> null_val = SafeInvoke(null_leaf).Then(&Leaf::GetValue);
  EXPECT_FALSE(null_val.has_value());
}

TEST(SafeInvokeUnitTest, WithArgsVoidReturn) {
  Leaf leaf;
  leaf.value = 5;

  SafeInvoke(&leaf).Then(&Leaf::Add, 10);
  EXPECT_EQ(leaf.value, 15);

  Leaf* null_leaf = nullptr;
  SafeInvoke(null_leaf).Then(&Leaf::Add, 10);
}

TEST(SafeInvokeUnitTest, WithArgsValueReturn) {
  Branch branch;
  branch.branch_id = 7;

  std::optional<int> multiplied =
      SafeInvoke(&branch).Then(&Branch::MultiplyBranchId, 6);
  ASSERT_TRUE(multiplied.has_value());
  EXPECT_EQ(multiplied.value(), 42);

  Branch* null_branch = nullptr;
  std::optional<int> null_multiplied =
      SafeInvoke(null_branch).Then(&Branch::MultiplyBranchId, 6);
  EXPECT_FALSE(null_multiplied.has_value());
}

TEST(SafeInvokeUnitTest, SubclassInheritanceDispatch) {
  DerivedView derived;
  SafeInvoke(&derived).Then(&BaseView::SetVisible, false);
  EXPECT_FALSE(derived.visible);

  std::optional<bool> vis = SafeInvoke(&derived).Then(&BaseView::IsVisible);
  ASSERT_TRUE(vis.has_value());
  EXPECT_FALSE(vis.value());

  DerivedView* null_derived = nullptr;
  SafeInvoke(null_derived).Then(&BaseView::SetVisible, true);
  std::optional<bool> null_vis =
      SafeInvoke(null_derived).Then(&BaseView::IsVisible);
  EXPECT_FALSE(null_vis.has_value());
}

// -----------------------------------------------------------------------------
// References & Move-Only Semantics
// -----------------------------------------------------------------------------

TEST(SafeInvokeUnitTest, ReferenceReturnWrapsInSafeChainAndAllowsChaining) {
  Tree tree;
  tree.branch_member.branch_id = 88;

  int id = SafeInvoke(&tree)
               .Then(&Tree::GetBranchRef)
               .Then(&Branch::GetBranchId)
               .value_or(-1);

  EXPECT_EQ(id, 88);
}

TEST(SafeInvokeUnitTest, MoveOnlyTypeReferenceReturn) {
  Branch branch;
  branch.move_only.value = std::make_unique<int>(123);

  MoveOnlyData* data = SafeInvoke(&branch).Then(&Branch::GetMoveOnly).get();
  ASSERT_NE(data, nullptr);
  ASSERT_NE(data->value, nullptr);
  EXPECT_EQ(*data->value, 123);
}

TEST(SafeInvokeUnitTest, OverloadedAddressOfOperatorSafety) {
  Branch branch;
  branch.overloaded_addr.value = 999;

  OverloadedAddressOf* addr_obj =
      SafeInvoke(&branch).Then(&Branch::GetOverloadedAddr).get();
  ASSERT_NE(addr_obj, nullptr);
  EXPECT_EQ(addr_obj->value, 999);
}

// -----------------------------------------------------------------------------
// Callables: Free Functions, Functors, and Chromium Callbacks
// -----------------------------------------------------------------------------

TEST(SafeInvokeUnitTest, FreeFunctionNavigation) {
  Branch branch;
  branch.leaf->value = 50;

  int score = SafeInvoke(&branch)
                  .Then(&FreeGetLeaf)
                  .Then(&FreeComputeScore, 3)
                  .value_or(0);

  EXPECT_EQ(score, 150);
}

TEST(SafeInvokeUnitTest, FunctorCallable) {
  Leaf leaf;
  leaf.value = 21;

  SafeInvoke(&leaf).Then(LeafDoubler{});
  EXPECT_EQ(leaf.value, 42);
}

TEST(SafeInvokeUnitTest, ChromiumRepeatingCallback) {
  Leaf leaf;
  leaf.value = 100;

  auto callback =
      base::BindRepeating([](Leaf* l, int delta) { l->Add(delta); });
  SafeInvoke(&leaf).Then(callback, 25);
  EXPECT_EQ(leaf.value, 125);
}

TEST(SafeInvokeUnitTest, ChromiumOnceCallback) {
  Leaf leaf;
  leaf.value = 200;

  auto once_callback =
      base::BindOnce([](Leaf* l, int delta) { l->Add(delta); });
  SafeInvoke(&leaf).Then(std::move(once_callback), 50);
  EXPECT_EQ(leaf.value, 250);
}

TEST(SafeInvokeUnitTest, PointerLambdaCallableDispatch) {
  Leaf leaf;
  leaf.value = 10;

  bool executed = false;
  SafeInvoke(&leaf).Then([&executed](Leaf*) { executed = true; });
  EXPECT_TRUE(executed);

  executed = false;
  Leaf* null_leaf = nullptr;
  SafeInvoke(null_leaf).Then([&executed](Leaf*) { executed = true; });
  EXPECT_FALSE(executed);
}

// -----------------------------------------------------------------------------
// Multi-Step Chaining & Type Unwrapping
// -----------------------------------------------------------------------------

TEST(SafeInvokeUnitTest, MultiStepChainPointerReturn) {
  Tree tree;
  tree.branch->leaf->value = 77;

  int final_val = SafeInvoke(&tree)
                      .Then(&Tree::GetBranch)
                      .Then(&Branch::GetLeaf)
                      .Then(&Leaf::GetValue)
                      .value_or(-1);

  EXPECT_EQ(final_val, 77);

  // Short-circuiting with middle null:
  tree.branch.reset();
  int null_val = SafeInvoke(&tree)
                     .Then(&Tree::GetBranch)
                     .Then(&Branch::GetLeaf)
                     .Then(&Leaf::GetValue)
                     .value_or(-1);

  EXPECT_EQ(null_val, -1);
}

TEST(SafeInvokeUnitTest, MultiStepChainVoidMethod) {
  Tree tree;
  tree.branch->leaf->value = 10;

  SafeInvoke(&tree)
      .Then(&Tree::GetBranch)
      .Then(&Branch::GetLeaf)
      .Then(&Leaf::Add, 5);

  EXPECT_EQ(tree.branch->leaf->value, 15);

  tree.branch.reset();
  SafeInvoke(&tree)
      .Then(&Tree::GetBranch)
      .Then(&Branch::GetLeaf)
      .Then(&Leaf::Add, 5);
}

TEST(SafeInvokeUnitTest, SmartPointersAndRawPtr) {
  std::unique_ptr<Tree> tree_unique = std::make_unique<Tree>();
  tree_unique->branch->leaf->value = 30;

  int val1 = SafeInvoke(tree_unique)
                 .Then(&Tree::GetBranch)
                 .Then(&Branch::GetLeaf)
                 .Then(&Leaf::GetValue)
                 .value_or(0);
  EXPECT_EQ(val1, 30);

  raw_ptr<Tree> tree_raw = tree_unique.get();
  int val2 = SafeInvoke(tree_raw)
                 .Then(&Tree::GetBranch)
                 .Then(&Branch::GetLeaf)
                 .Then(&Leaf::GetValue)
                 .value_or(0);
  EXPECT_EQ(val2, 30);
}

TEST(SafeInvokeUnitTest, IntermediateWeakPtrChaining) {
  Branch branch;
  branch.weak_leaf->value = 55;

  std::optional<int> val =
      SafeInvoke(&branch).Then(&Branch::GetWeakLeaf).Then(&WeakLeaf::GetValue);
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(val.value(), 55);

  // Calling a modifying void method through a WeakPtr:
  SafeInvoke(&branch).Then(&Branch::GetWeakLeaf).Then(&WeakLeaf::Add, 10);
  EXPECT_EQ(branch.weak_leaf->value, 65);

  // Extracting raw pointer via .get():
  WeakLeaf* raw_leaf = SafeInvoke(&branch).Then(&Branch::GetWeakLeaf).get();
  EXPECT_EQ(raw_leaf, branch.weak_leaf.get());
}

TEST(SafeInvokeUnitTest, IntermediateWeakPtrInvalidatedAndNull) {
  Branch branch;
  branch.weak_leaf->value = 88;
  branch.stored_weak_leaf = branch.weak_leaf->AsWeakPtr();

  // Invalidate the weak pointer:
  branch.weak_leaf->InvalidateWeakPtrs();

  std::optional<int> val = SafeInvoke(&branch)
                               .Then(&Branch::GetStoredWeakLeaf)
                               .Then(&WeakLeaf::GetValue);
  EXPECT_FALSE(val.has_value());

  // Also test when pointee object is destroyed:
  branch.stored_weak_leaf = branch.weak_leaf->AsWeakPtr();
  branch.weak_leaf.reset();
  std::optional<int> destroyed_val = SafeInvoke(&branch)
                                         .Then(&Branch::GetStoredWeakLeaf)
                                         .Then(&WeakLeaf::GetValue);
  EXPECT_FALSE(destroyed_val.has_value());

  // Method returning explicitly null WeakPtr:
  std::optional<int> null_val = SafeInvoke(&branch)
                                    .Then(&Branch::GetNullWeakLeaf)
                                    .Then(&WeakLeaf::GetValue);
  EXPECT_FALSE(null_val.has_value());

  // Chain starting from null Branch:
  Branch* null_branch = nullptr;
  std::optional<int> null_branch_val = SafeInvoke(null_branch)
                                           .Then(&Branch::GetWeakLeaf)
                                           .Then(&WeakLeaf::GetValue);
  EXPECT_FALSE(null_branch_val.has_value());
}

TEST(SafeInvokeUnitTest, SafeInvokeWithInvalidWeakPtrRoot) {
  WeakLeaf leaf;
  leaf.value = 42;
  base::WeakPtr<WeakLeaf> weak_ptr = leaf.AsWeakPtr();
  leaf.InvalidateWeakPtrs();

  // Passing an invalidated WeakPtr at the root of SafeInvoke should safely
  // short-circuit without crashing or CHECK-failing.
  std::optional<int> val = SafeInvoke(weak_ptr).Then(&WeakLeaf::GetValue);
  EXPECT_FALSE(val.has_value());
}

TEST(SafeInvokeUnitTest, IntermediateScopedRefptrChaining) {
  Branch branch;
  branch.ref_leaf->value = 70;

  // Value return:
  std::optional<int> val =
      SafeInvoke(&branch).Then(&Branch::GetRefLeaf).Then(&RefLeaf::GetValue);
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(val.value(), 70);

  // Reference return (const scoped_refptr<T>&):
  std::optional<int> const_ref_val = SafeInvoke(&branch)
                                         .Then(&Branch::GetConstRefLeaf)
                                         .Then(&RefLeaf::GetValue);
  ASSERT_TRUE(const_ref_val.has_value());
  EXPECT_EQ(const_ref_val.value(), 70);

  // Extracting raw pointer via .get():
  RefLeaf* raw_ref = SafeInvoke(&branch).Then(&Branch::GetRefLeaf).get();
  EXPECT_EQ(raw_ref, branch.ref_leaf.get());
}

TEST(SafeInvokeUnitTest, IntermediateScopedRefptrNull) {
  Branch branch;

  // Method returning null scoped_refptr:
  std::optional<int> null_val = SafeInvoke(&branch)
                                    .Then(&Branch::GetNullRefLeaf)
                                    .Then(&RefLeaf::GetValue);
  EXPECT_FALSE(null_val.has_value());

  // Chain starting from null branch:
  Branch* null_branch = nullptr;
  std::optional<int> null_branch_val = SafeInvoke(null_branch)
                                           .Then(&Branch::GetRefLeaf)
                                           .Then(&RefLeaf::GetValue);
  EXPECT_FALSE(null_branch_val.has_value());
}

TEST(SafeInvokeUnitTest, ContextualBooleanConversion) {
  Tree tree;
  Tree* null_tree = nullptr;

  EXPECT_TRUE(SafeInvoke(&tree));
  EXPECT_FALSE(SafeInvoke(null_tree));

  EXPECT_TRUE(SafeInvoke(&tree).Then(&Tree::GetBranch));
  EXPECT_FALSE(SafeInvoke(&tree).Then(&Tree::GetNullBranch));
}

TEST(SafeInvokeUnitTest, NoDoubleWrappingOptional) {
  Leaf leaf;
  leaf.value = 42;

  std::optional<int> val =
      SafeInvoke(&leaf).Then(&Leaf::GetOptionalValue, true);
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(val.value(), 42);

  std::optional<int> none =
      SafeInvoke(&leaf).Then(&Leaf::GetOptionalValue, false);
  EXPECT_FALSE(none.has_value());
}

// -----------------------------------------------------------------------------
// Overload and ConstOverload Disambiguation
// -----------------------------------------------------------------------------

TEST(SafeInvokeUnitTest, OverloadMemberFunctionNonConst) {
  Leaf leaf{.value = 100};

  std::optional<int> result =
      SafeInvoke(&leaf).Then(Overload<int>(&Leaf::Compute), 5);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 105);
}

TEST(SafeInvokeUnitTest, ConstOverloadMemberFunction) {
  const Leaf leaf{.value = 100};

  std::optional<int> result =
      SafeInvoke(&leaf).Then(ConstOverload<int>(&Leaf::Compute), 5);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 110);
}

TEST(SafeInvokeUnitTest, OverloadMemberFunctionArity) {
  Leaf leaf{.value = 20};

  std::optional<int> res0 = SafeInvoke(&leaf).Then(Overload<>(&Leaf::Process));
  ASSERT_TRUE(res0.has_value());
  EXPECT_EQ(res0.value(), 20);

  std::optional<int> res1 =
      SafeInvoke(&leaf).Then(Overload<int>(&Leaf::Process), 10);
  ASSERT_TRUE(res1.has_value());
  EXPECT_EQ(res1.value(), 30);

  std::optional<int> res2 =
      SafeInvoke(&leaf).Then(Overload<int, int>(&Leaf::Process), 10, 3);
  ASSERT_TRUE(res2.has_value());
  EXPECT_EQ(res2.value(), 90);
}

TEST(SafeInvokeUnitTest, OverloadFreeFunction) {
  Branch branch;
  branch.branch_id = 5;

  std::optional<int> res1 =
      SafeInvoke(&branch).Then(Overload<Branch*>(&FreeCompute));
  ASSERT_TRUE(res1.has_value());
  EXPECT_EQ(res1.value(), 15);

  std::optional<int> res2 =
      SafeInvoke(&branch).Then(Overload<Branch*, int>(&FreeCompute), 20);
  ASSERT_TRUE(res2.has_value());
  EXPECT_EQ(res2.value(), 25);
}

// -----------------------------------------------------------------------------
// Custom Value Types & Rvalue Safety
// -----------------------------------------------------------------------------

TEST(SafeInvokeUnitTest, NonOptionalTypeWithHasValueMethod) {
  Branch branch;
  branch.branch_id = 42;

  std::optional<CustomStatusWithHasValue> res = SafeInvoke(&branch).Then(
      [](Branch* b) { return CustomStatusWithHasValue{.code = b->branch_id}; });
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(res.value().code, 42);

  Branch* null_branch = nullptr;
  std::optional<CustomStatusWithHasValue> null_res =
      SafeInvoke(null_branch).Then([](Branch* b) {
        return CustomStatusWithHasValue{.code = b->branch_id};
      });
  EXPECT_FALSE(null_res.has_value());
}

TEST(SafeInvokeUnitTest, RvalueQualificationSafety) {
  static_assert(!std::is_copy_constructible_v<SafeChain<Leaf>>);
  static_assert(!std::is_copy_assignable_v<SafeChain<Leaf>>);
  static_assert(!std::is_move_constructible_v<SafeChain<Leaf>>);
  static_assert(!std::is_move_assignable_v<SafeChain<Leaf>>);

  Leaf leaf{.value = 10};

  // Rvalue chaining works on temporaries:
  EXPECT_EQ(SafeInvoke(&leaf).Then(&Leaf::GetValue).value_or(-1), 10);
  SafeInvoke(&leaf).Then(&Leaf::Increment);
  EXPECT_EQ(SafeInvoke(&leaf).get(), &leaf);
}

}  // namespace
