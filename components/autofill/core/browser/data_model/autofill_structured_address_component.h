// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_COMPONENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_COMPONENT_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_parsing_expression_components.h"
#include "components/autofill/core/browser/field_types.h"

namespace re2 {
class RE2;
}  // namespace re2

namespace autofill {

struct AddressToken;
struct SortedTokenComparisonResult;

// Represents the validation status of value stored in the AutofillProfile.
// The associated integer values used to store the verification code in SQL and
// should not be modified.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
enum class VerificationStatus {
  // No verification status assigned.
  kNoStatus = 0,
  // The value token was parsed from a parent token.
  kParsed = 1,
  // Value was built from its subcomponents.
  kFormatted = 2,
  // The value was observed in a form transmission.
  kObserved = 3,
  // The user used the autofill settings to verify and store this token.
  kUserVerified = 4,
  // The token was parsed by the server.
  kServerParsed = 5,
  kMaxValue = kServerParsed
};

// Prints the string representation of |status| to |os|.
std::ostream& operator<<(std::ostream& os, VerificationStatus status);

// Returns true if |left| has a less significant verification status compared to
// |right|.
bool IsLessSignificantVerificationStatus(VerificationStatus left,
                                         VerificationStatus right);

// Returns the more significant verification status according to
// |IsLessSignificantVerificationStatus|.
VerificationStatus GetMoreSignificantVerificationStatus(
    VerificationStatus left,
    VerificationStatus right);

// The merge mode defines if and how two components are merged.
// The merge operations are applied in the order defined here.
// If one merge operation succeeds, the subsequent ones are not tested.
// Therefore, if |KUseBetterOrMoreRecentIfDifferent| is active,
// |kMergeChildrenAndReformatIfNeeded| will not be applied because
// |kUseBetterOrMostRecentIfDifferent| is always applicable.
enum MergeMode {
  // If one component has an empty value, use the non-empty one.
  kReplaceEmpty = 1,
  // Recursively merge two components that have the same tokens in arbitrary
  // order. This is used as the default merge mode.
  kRecursivelyMergeTokenEquivalentValues = 1 << 1,
  // If both tokens have the same normalized value, use the one with the better
  // verification status. If both statuses are the same, use the newer one.
  kUseBetterOrNewerForSameValue = 1 << 2,
  // If one component is a superset of the other, use the subset.
  kReplaceSuperset = 1 << 3,
  // If one component is a subset of the other, use the superset.
  kReplaceSubset = 1 << 4,
  // If both components have a different value, is the newer one.
  kUseNewerIfDifferent = 1 << 5,
  // If the newer component contains one token more, apply a recursive strategy
  // to merge the tokens.
  kRecursivelyMergeSingleTokenSubset = 1 << 6,
  // If one is a substring of the other use the most recent one.
  kUseMostRecentSubstring = 1 << 7,
  // If the tokens match or one is a subset of the other, pick the shorter one.
  kPickShorterIfOneContainsTheOther = 1 << 8,
  // If the normalized values are different, use the better one in terms
  // of verification score or the most recent one if both scores are the same.
  kUseBetterOrMostRecentIfDifferent = 1 << 9,
  // Merge the child nodes and reformat the node from its children after merge
  // if the value has changed.
  kMergeChildrenAndReformatIfNeeded = 1 << 10,
  // Make a merge decision based on canonicalized values.
  kMergeBasedOnCanonicalizedValues = 1 << 11,
  // Defines the default merging behavior.
  kDefault = kRecursivelyMergeTokenEquivalentValues
};

// An AddressComponent is a tree structure that represents a semi-structured
// address token. Such an address token can either be an atomic leaf node or
// have a set of children, each representing a more granular subtoken of the
// component.
//
// An AddressComponent has a string representation stored in |value_| and a
// VerificationStatus stored in |verification_status_|.
// The latter indicates if the value was user-verified, observed in a form
// submission event, parsed from its parent component or was formatted from its
// child components.
//
// In a proper component tree, each AddressComponent has a unique
// FieldType. Additionally, an AddressComponent may be associated with a
// list of additional field types that allow for retrieving and setting the
// Component's value in specific formats. For example, NAME_MIDDLE may be the
// storage type and NAME_MIDDLE_INITIAL is an additional field type.
//
// The usage pattern of such an address tree is as follows:
//
//  * Create a tree from an observed form submission or a profile editing or
//  creation event in the Chrome settings. It is assumed that the created
//  tree does not have values for competing field types. Two types are competing
//  iff they are on a common root-to-leaf path. For example, an imported profile
//  with a value for NAME_FULL and NAME_LAST has conflicting types that
//  carry redundant information.
//
//  * After the creation of the tree, the values of unassigned nodes in the tree
//  are deducted from the values of assigned nodes. This happens by parsing
//  (taking a string and splitting it into components) or by formatting (taking
//  one or multiple strings and combining them into one string).
//
//  * After the completion, there should be no need to modify the tree.
//
//  * A tree may be mergeable with another tree of the same type. This
//  operation incorporates complementing observations. For example, in the first
//  tree NAME_FIRST, NAME_MIDDLE and NAME_LAST may be parsed from an observed
//  unstructured name (NAME_FULL). The second tree may be built from observing
//  the structured name, and contain observed NAME_FIRST, NAME_MIDDLE and
//  NAME_LAST values but only a formatted NAME_FULL value.
class AddressComponent {
 public:
  // List of node subcomponents.
  //
  // Nodes within the tree-like structure do not directly manage the lifetime
  // of their child nodes. Instead, the AddressComponentsStore serves as the
  // central owner and manager of AddressComponent nodes. When the
  // AddressComponentsStore is being destroyed, the WipeRawPtrsForDestruction
  // function is used to clear the list of child pointers. This prevents
  // dangling pointers during the destruction process.
  using SubcomponentsList = std::vector<AddressComponent*>;

  // Constructor for a compound child node.
  AddressComponent(FieldType storage_type,
                   SubcomponentsList subcomponents,
                   unsigned int merge_mode);

  // Disallows copies and direct assignments since they are not needed in the
  // current Autofill design.
  AddressComponent(const AddressComponent& other) = delete;
  AddressComponent& operator=(const AddressComponent& right) = delete;

  virtual ~AddressComponent();

  // Migrates from a legacy structure in which tokens are imported without
  // a status.
  virtual void MigrateLegacyStructure() {}

  // Comparison operators are deleted in favor of and |SameAs()|.
  bool operator==(const AddressComponent& right) const = delete;
  bool operator!=(const AddressComponent& right) const = delete;

  // Compares the values and verification statuses with |other| recursively
  // down the tree. Returns true iff all values and verification statuses of
  // this node and its subtree and |other| with its subtree are the same.
  bool SameAs(const AddressComponent& other) const;

  // Copies the values and verification statuses from |other| recursively down
  // the tree.
  void CopyFrom(const AddressComponent& other);

  // Returns the autofill storage type stored in |storage_type_|.
  FieldType GetStorageType() const;

  // Returns the type to be used instead of `field_type` when this type does
  // not contain information. It is assumed that `field_type` is a supported
  // type of the node,
  FieldType GetFallbackType(FieldType field_type) const;

  // Returns the string representation of |storage_type_|.
  std::string GetStorageTypeName() const;

  // Returns the value verification status of the component's value;
  VerificationStatus GetVerificationStatus() const;

  // Returns true if the component has no subcomponents.
  bool IsAtomic() const;

  // Returns a constant reference to |value_.value()|. If the value is not
  // assigned, an empty string is returned.
  const std::u16string& GetValue() const;

  // Returns a canonicalized version of the value or std::nullopt if
  // canonicalization is not possible or not implemented.
  virtual std::optional<std::u16string> GetCanonicalizedValue() const;

  // Returns true if the value of this AddressComponent is assigned.
  bool IsValueAssigned() const;

  // Sets the value corresponding to the storage type of this AddressComponent.
  virtual void SetValue(std::u16string value, VerificationStatus status);

  // Wrapper function around
  // SetValueForTypeIfPossible(/*invalidate_child_nodes=*/false);
  bool SetValueForType(FieldType field_type,
                       const std::u16string& value,
                       const VerificationStatus& status);

  // Wrapper function around
  // SetValueForTypeIfPossible(/*invalidate_child_nodes=*/true);
  // TODO(crbug.com/40266145): Remove and merge with SetValueForType.
  bool SetValueForTypeAndResetSubstructure(FieldType field_type,
                                           const std::u16string& value,
                                           const VerificationStatus& status);

  // Sets the value to an empty string, marks it unassigned and sets the
  // verification status to |kNoStatus|.
  virtual void UnsetValue();

  // Convenience method to get the value of |field_type|.
  // Returns an empty string if |field_type| is not supported.
  std::u16string GetValueForType(FieldType field_type) const;

  // Convenience method to get the value of `field_type` to be used for
  // comparison with `other`. Returns an empty string if `field_type` is not
  // supported.
  std::u16string GetValueForComparisonForType(
      FieldType field_type,
      const AddressComponent& other) const;

  // Convenience method to get the verification status of `field_type`.
  // Returns |VerificationStatus::kNoStatus| if `field_type` is not supported.
  VerificationStatus GetVerificationStatusForType(FieldType field_type) const;

  // Convenience method to get the fallback type of a specific node whose
  // supported type include `field_type`. Returns `field_type` in case it could
  // not find a `field_type` node.
  FieldType GetFallbackTypeForType(FieldType field_type) const;

  // Returns true if the |value| and |verification_status| were successfully
  // unset for |type|.
  bool UnsetValueForTypeIfSupported(FieldType field_type);

  // Parses |value_| to assign values to the subcomponents.
  // The method uses 2 stages:
  //
  // * Use |ParseValueAndAssignSubcomponentsByRegularExpressions()|. This stage
  // uses a list of regular expressions acquired by the virtual method
  // |GetParseRegularExpressionsByRelevance()|. This stage my fail.
  //
  // * Use |ParseValueAndAssignSubcomponentsByFallbackMethod()| as the last
  // resort to parse |value_|. This method must produce a valid result.
  void ParseValueAndAssignSubcomponents();

  // This methods populated the unassigned entries in the subtree of this node
  // by either parsing unknown values for subcomponents from their parents, or
  // vice versa, formatting unknown values from known subcomponents. The method
  // is virtual and can be reimplemented on the type level.
  virtual void RecursivelyCompleteTree();

  // Completes the full tree by calling |RecursivelyCompleteTree()| starting
  // form the root node. Returns true if the completion was successful.
  virtual bool CompleteFullTree();

  // Generates values for the tree's synthesized nodes recursively, starting
  // from the root. Values for synthesized nodes are generated using formatting
  // expressions.
  void GenerateTreeSynthesizedNodes();

  // Checks if a tree is completable in the sense that there are no conflicting
  // observed or verified types. This means that there is not more than one
  // observed or verified node on any root-to-leaf path in the tree.
  bool IsTreeCompletable();

  // Recursively adds the supported types to the set. Calls
  // |GetAdditionalSupportedFieldTypes()| to add field types.
  void GetSupportedTypes(FieldTypeSet* supported_types) const;

  // Recursively adds only the storable types to the set. No computed type is
  // ever added (e.g. GetAdditionalSupportedFieldTypes).
  void GetStorableTypes(FieldTypeSet* supported_types) const;

  // Recursively finds the storable type of `type`:
  // - If `type` is a `storable_type_` of any node, this is simply `type`.
  // - If `type` is an additional supported type of any node, this is the
  //   storable type of that node.
  // - Otherwise, if `type` is not a supported type of any node, return nullopt.
  std::optional<FieldType> GetStorableTypeOf(FieldType type) const;

  // Adds the additional supported field types to |supported_types|.
  // The method should DCHECK that the added types are not part of the set yet.
  virtual const FieldTypeSet GetAdditionalSupportedFieldTypes() const;

  // Unassigns all nodes with parsed or formatted values.
  void UnsetParsedAndFormattedValuesInEntireTree();

  // Unassigns all nodes with parsed or formatted values.
  void RecursivelyUnsetParsedAndFormattedValues();

  // Returns true if both components are mergeable.
  virtual bool IsMergeableWithComponent(
      const AddressComponent& newer_component) const;

  // Recursively updates the verification statuses to the higher one, for nodes
  // in |newer_component| that have the same values as the nodes in |this|.
  virtual void MergeVerificationStatuses(
      const AddressComponent& newer_component);

  // Merge |newer_component| into this AddressComponent.
  // Returns false if the merging is not possible.
  // The state of the component is not altered by a failed merging attempt.
  // |newer_was_more_recently_used| indicates that the newer component was also
  // more recently used for filling a form.
  virtual bool MergeWithComponent(const AddressComponent& newer_component,
                                  bool newer_was_more_recently_used = true);

  // Merge |newer_component| into this AddressComponent.
  // The merging is possible iff the value of both root nodes is token
  // equivalent, meaning they contain the same tokens in an arbitrary order.
  // Returns false if the merging is not possible.
  // The state of the component is not altered by a failed merging attempt.
  bool MergeTokenEquivalentComponent(const AddressComponent& newer_component);

  // Returns a constant vector of pointers to the child nodes of the component.
  const SubcomponentsList& Subcomponents() const { return subcomponents_; }

  // Returns a constant vector of pointers to the synthesized child nodes of the
  // component.
  const SubcomponentsList& SynthesizedSubcomponents() const {
    return synthesized_subcomponents_;
  }

  // Returns a pointer to the parent node.
  AddressComponent* Parent() const { return parent_; }

  // Register a new synthesized component in the `synthesized_subcomponents_`
  // list. Note that similarly as default subcomponents, synthesized
  // subcomponents are owned by the corresponding `AddressComponentsStore` and
  // not by `this`
  void RegisterSynthesizedSubcomponent(AddressComponent* synthesized_component);

  // Returns a vector containing sorted normalized tokens of the
  // value of the component. The tokens are lazily calculated when first
  // needed.
  const std::vector<AddressToken> GetSortedTokens() const;

  // Recursively unsets all subcomponents.
  void RecursivelyUnsetSubcomponents();

  // Return if the value associated with |field_type| is valid.
  // If |wipe_if_not|, the value is unset if invalid.
  bool IsValueForTypeValid(FieldType field_type, bool wipe_if_not = false);

  // While processing two structured addresses, if only one of them has their
  // country set, the other should assume the non-empty one while merging. This
  // is required to do consistent address rewriting.
  // Returns the common country to be used.
  AddressCountryCode GetCommonCountry(const AddressComponent& other) const;

  // If the tree this node is part of contains country code information, this
  // function retrieves it. Otherwise it returns an empty country code.
  AddressCountryCode GetCountryCode() const;

  // Deletes the stored structure and returns true if |IsStructureValid()|
  // returns false.
  virtual bool WipeInvalidStructure();

  // Returns if the structure in the tree below this node is valid. A structure
  // becomes invalid when it contains information that is not contained in the
  // value of this node.
  bool IsStructureValid() const;

  // Returns true if all values of all descendent nodes are empty.
  bool AllDescendantsAreEmpty() const;

  // Wipes internal pointers to guarantee that no pointers are kept to other
  // potentially destructed nodes.
  void WipeRawPtrsForDestruction();

 protected:
  // Returns the verification score of this component and its substructure.
  // Each observed node contributes to the validation score by 1.
  virtual int GetStructureVerificationScore() const;

  // Returns whether `field_type` is a supported type for the current node.
  bool IsSupportedType(FieldType field_type) const;

  // Returns whether the node's value is read-only. If true, the node's value
  // can only be set internally via `SetValue` but not from a caller outside of
  // the AddressComponent tree.
  virtual bool IsValueReadOnly() const;

  // Returns a vector containing the |storage_types_| of all direct
  // subcomponents.
  std::vector<FieldType> GetSubcomponentTypes() const;

  // Setter for the component's parent.
  void SetParent(AddressComponent* parent) { parent_ = parent; }

  // Heuristic method to get the format string suited for the node's type and
  // country.
  virtual std::u16string GetFormatString() const;

  // Returns pointers to regular expressions sorted by their relevance.
  virtual std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance()
      const;

  // This method parses |value_| to assign values to the subcomponents.
  // The method is virtual and can be reimplemented per type.
  // It must succeed.
  virtual void ParseValueAndAssignSubcomponentsByFallbackMethod();

  // This method is used to set the value given by a type different than the
  // storage type. It must implement the conversion logic specific to each
  // type.
  virtual void SetValueForOtherSupportedType(FieldType field_type,
                                             const std::u16string& value,
                                             const VerificationStatus& status);

  // This method is used to retrieve the value for a supported field type
  // different from the storage type. It must implement the conversion logic
  // specific to each type.
  virtual std::u16string GetValueForOtherSupportedType(
      FieldType field_type) const;

  // Clears all parsed and formatted values.
  void ClearAllParsedAndFormattedValues();

  // Merge a component that has exactly one token less.
  bool MergeSubsetComponent(
      const AddressComponent& subset_component,
      const SortedTokenComparisonResult& token_comparison_result);

  // Consumes an additional token into the most appropriate subcomponent.
  // Can be implemented by the specific node types.
  // The fall-back solution uses the first empty node.
  // If no empty node is available, it appends the value to the first node.
  virtual void ConsumeAdditionalToken(const std::u16string& token_value);

  // Returns a reference to the root node of the tree.
  AddressComponent& GetRootNode();

  // Returns a reference to the root node of the tree.
  const AddressComponent& GetRootNode() const;

  // Function to determine if the value stored in this component is valid.
  // Return true be default but can be overloaded by a subclass.
  virtual bool IsValueValid() const;

  // Function to be called post assign to do sanitization.
  virtual void PostAssignSanitization() {}

  // Returns a normalized value for comparison.
  // In the default implementation, this converts the value to lower case and
  // removes white spaces. This function may be reimplemented to perform
  // different normalization operations.
  virtual std::u16string GetNormalizedValue() const;

  // Returns a value used for comparison.
  // In the default implementation this is just the normalized value but this
  // function can be overridden in subclasses to apply further operations on
  // the normalized value.
  // |other| represents the component we are comparing with and is required
  // for consistent rewriting rules.
  std::u16string GetValueForComparison(const AddressComponent& other) const;

  // Formats `value` to be used for comparison.
  // In the default implementation this is `value` normalized but this function
  // can be overridden in subclasses to apply further operations on `value`.
  // `other` represents the component we are comparing with and is required
  // for consistent rewriting rules.
  virtual std::u16string GetValueForComparison(
      const std::u16string& value,
      const AddressComponent& other) const;

  // Returns true if the merging of two token identical values should give
  // precedence to the newer value.
  virtual bool HasNewerValuePrecedenceInMerging(
      const AddressComponent& newer_component) const;

  // Parses |value| by using |parse_expressions| and assigns the values.
  // Returns true on success.
  bool ParseValueAndAssignSubcomponentsByRegularExpression(
      const std::u16string& value,
      const re2::RE2* parse_expression);

  // Determines and sets a formatted value using
  // |GetFormattedValueFromSubcomponents|.
  void FormatValueFromSubcomponents();

  // Returns the maximum number of components with assigned values on the path
  // from the component to a leaf node.
  int MaximumNumberOfAssignedAddressComponentsOnNodeToLeafPaths() const;

  // Function to be called by nodes to register new children in their
  // `subcomponents_` list. Note that children are owned by the corresponding
  // `AddressComponentsStore` and not by `this`.
  // `set_as_parent_of_child` is used to prevent the child from setting its
  // parent pointer to `this` when the child is already a child of another node.
  // This is the case for the children of synthesized nodes that don't get a new
  // parent assigned.
  void RegisterChildNode(AddressComponent* child,
                         bool set_as_parent_of_child = true);

  // Returns the node in the tree that supports `field_type`. This node, if it
  // exists, is unique by definition. Returns nullptr if no such node exists.
  AddressComponent* GetNodeForType(FieldType field_type);

  // const version of GetNodeForType.
  const AddressComponent* GetNodeForType(FieldType field_type) const;

  // Recursively adds the supported types to the set. If `!storable_only`, calls
  // |GetAdditionalSupportedFieldTypes()| to add computed field types.
  virtual void GetTypes(bool storable_only,
                        FieldTypeSet* supported_types) const;

 private:
  friend class AddressComponentTestApi;

  // Unsets the node and all of its children.
  void UnsetAddressComponentAndItsSubcomponents();

  // Unsets the children of a node.
  void UnsetSubcomponents();

  // In cases where the tree has been initially completed, there might still be
  // nodes that are empty (e.g. a new leaf or internal node got recently
  // introduced). Gap filling addresses all those cases.
  // The overall strategy is: For every empty node, try building its value by
  // parsing its parent. If that's not possible (e.g. info can't be parsed),
  // use formatting rules to build a value from its children. Note that this
  // process respects non-empty nodes and the information growth invariant (i.e
  // child information is always contained on their ancestors).
  void FillTreeGaps();

  // Determines a value from the subcomponents by using the
  // most suitable format string determined by |GetBestFormatString()|.
  std::u16string GetFormattedValueFromSubcomponents();

  // Replaces placeholder values with the corresponding values.
  std::u16string ReplacePlaceholderTypesWithValues(
      std::u16string_view format) const;

  // This method uses i18n parsing instructions used by
  // `ParseValueByI18nRegularExpression` to parse |value_| into the values of
  // the subcomponents. Returns true on success and is allowed to fail.
  bool ParseValueAndAssignSubcomponentsByI18nParsingRules();

  // This method uses regular expressions acquired by
  // |GetParseRegularExpressionsByRelevance| to parse |value_| into the values
  // of the subcomponents. Returns true on success and is allowed to fail.
  bool ParseValueAndAssignSubcomponentsByRegularExpressions();

  // This method uses regular expressions acquired by
  // |GetParseRegularExpressionsByRelevance| to parse |value_| into the values
  // of the subcomponents that are empty, components with non-empty values
  // remain unchanged. If parsing is not successful, the function does not
  // perform any modifications. Returns true if parsing was successful.
  void TryParseValueAndAssignSubcomponentsRespectingSetValues();

  // Parses |value| by using |parse_expressions| and assigns values to empty
  // subcomponents only. The value assigned to each subcomponent is compatible
  // with the information growth invariant (i.e child information is always
  // contained on their ancestors). If parsing is not successful, the function
  // does not perform any modifications. Returns true if parsing was
  // successful.
  bool ParseValueAndAssignSubcomponentsRespectingSetValues(
      const std::u16string& value,
      const re2::RE2* parse_expression);

  // Assigns parsing results to the corresponding subcomponents. Overrides
  // previously set values if necessary.
  void AssignParsedValuesToSubcomponents(
      i18n_model_definition::ValueParsingResults values);

  // Assigns parsing results to the corresponding subcomponents. The value
  // assigned to each subcomponent must be compatible with the information
  // growth invariant (i.e child information is always contained in their
  // ancestors). Previously set values are not overridden.
  bool AssignParsedValuesToSubcomponentsRespectingSetValues(
      i18n_model_definition::ValueParsingResults values);

  // This method verifies that the `value` is token compatible with this node
  // and all the node's descendants.
  bool IsValueCompatibleWithDescendants(const std::u16string& value) const;

  // This method verifies that the `value` is token compatible with this node
  // and all the node's ancestors.
  bool IsValueCompatibleWithAncestors(const std::u16string& value) const;

  // The unstructured value of this component.
  std::optional<std::u16string> value_;

  // The verification status of |value_| indicates the certainty of the value
  // to be correct.
  VerificationStatus value_verification_status_;

  // The storable Autofill type of the component.
  const FieldType storage_type_;

  // A vector of children of the component.
  SubcomponentsList subcomponents_;

  // A vector of synthesized nodes of the component.
  //
  // Nodes in the tree might have synthesized nodes linked to it. A synthesized
  // node, is a node whose value is calculated from its constituents. While
  // synthesized types are not directly part of the address hierarchy tree, its
  // constituents are. Synthesized nodes, similarly as normal subcomponents are
  // stored in the AddressComponentsStore.
  SubcomponentsList synthesized_subcomponents_;

  // A vector that contains the tokens of |value_| after normalization,
  // meaning that it was converted to lower case and diacritics have been
  // removed. |value_| is tokenized by splitting the string by white spaces and
  // commas. It is calculated when |value_| is set.
  std::optional<std::vector<AddressToken>> sorted_normalized_tokens_;

  // A pointer to the parent node. It is set to nullptr if the node is the root
  // node of the AddressComponent tree. Similarly to the `subcomponents_`, the
  // parent node is owned by the `AddressComponentsStore`. The pointer is
  // cleared during the destruction process of the `AddressComponentsStore` to
  // prevent dangling pointers to other nodes.
  raw_ptr<AddressComponent> parent_ = nullptr;

  // Defines if and how two components can be merged.
  int merge_mode_;
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_COMPONENT_H_
