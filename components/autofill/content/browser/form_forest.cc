// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/form_forest.h"

#include "base/containers/cxx20_erase_vector.h"
#include "base/containers/stack.h"
#include "base/debug/dump_without_crashing.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "components/autofill/content/browser/form_forest_util_inl.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

// AFCHECK(condition[, error_handler]) creates a crash dump and executes
// |error_handler| if |condition| is false.
// TODO(https://crbug.com/1187842): Replace AFCHECK() with DCHECK().
#if DCHECK_IS_ON()
#define AFCHECK(condition, ...)                                                \
  {                                                                            \
    SCOPED_CRASH_KEY_STRING256("autofill", "main_url", MainUrlForDebugging()); \
    DCHECK(condition);                                                         \
  }
#else
#define AFCHECK(condition, ...)                          \
  {                                                      \
    if (!(condition)) {                                  \
      SCOPED_CRASH_KEY_STRING256("autofill", "main_url", \
                                 MainUrlForDebugging()); \
      base::debug::DumpWithoutCrashing();                \
      __VA_ARGS__;                                       \
    }                                                    \
  }
#endif

namespace autofill {
namespace internal {

namespace {

// Indicates if |rfh| is a fenced frame (https://crbug.com/1111084).
//
// We do not want to fill across the boundary of a fenced frame. Hence, a fenced
// frame's FrameData must be disconnected (in terms of FormData::child_frames
// and FrameData::parent_form) from its parent form. This is already guaranteed
// because FormData::child_frames does not contain fenced frames. However,
// UpdateTreeOfRendererForm() would still invoke TriggerReparse() to detect the
// parent form. IsFencedFrameRoot() should be implemented to suppress this.
//
// We also do not want to fill across iframes with the disallowdocumentaccess
// attribute (https://crbug.com/961448). Since disallowdocumentaccess is
// currently not going to ship and supporting it requires significant additional
// work in UpdateTreeOfRendererForm() to remove FormData::child_frame and unset
// FrameData::parent_form for frames that disallow document access, there is no
// immediate need to support it. See https://crrev.com/c/3055422 for a draft
// implementation.
bool IsFencedFrameRoot(content::RenderFrameHost* rfh) {
  return rfh->IsFencedFrameRoot();
}

}  // namespace

FormForest::FrameData::FrameData(LocalFrameToken frame_token)
    : frame_token(frame_token) {}
FormForest::FrameData::~FrameData() = default;

FormForest::FormForest() = default;
FormForest::~FormForest() = default;

std::string FormForest::MainUrlForDebugging() const {
  content::RenderFrameHost* some_rfh = some_rfh_for_debugging_;
  if (!some_rfh) {
    for (const auto& frame_data : frame_datas_) {
      if (frame_data && frame_data->driver)
        some_rfh = frame_data->driver->render_frame_host();
    }
  }
  if (!some_rfh)
    return std::string();
  return some_rfh->GetMainFrame()->GetLastCommittedURL().spec();
}

absl::optional<LocalFrameToken> FormForest::Resolve(const FrameData& reference,
                                                    FrameToken query) {
  if (absl::holds_alternative<LocalFrameToken>(query))
    return absl::get<LocalFrameToken>(query);
  DCHECK(absl::holds_alternative<RemoteFrameToken>(query));
  AFCHECK(reference.driver, return absl::nullopt);
  content::RenderFrameHost* rfh = reference.driver->render_frame_host();
  AFCHECK(rfh, return absl::nullopt);
  content::RenderProcessHost* rph = rfh->GetProcess();
  AFCHECK(rph, return absl::nullopt);
  blink::RemoteFrameToken blink_remote_token(
      absl::get<RemoteFrameToken>(query).value());
  content::RenderFrameHost* remote_rfh =
      content::RenderFrameHost::FromPlaceholderToken(rph->GetID(),
                                                     blink_remote_token);
  if (!remote_rfh)
    return absl::nullopt;
  return LocalFrameToken(remote_rfh->GetFrameToken().value());
}

FormForest::FrameData* FormForest::GetOrCreateFrameData(LocalFrameToken frame) {
  auto it = frame_datas_.find(frame);
  if (it == frame_datas_.end())
    it = frame_datas_.insert(it, std::make_unique<FrameData>(frame));
  AFCHECK(it != frame_datas_.end());
  AFCHECK(it->get());
  return it->get();
}

FormForest::FrameData* FormForest::GetFrameData(LocalFrameToken frame) {
  auto it = frame_datas_.find(frame);
  return it != frame_datas_.end() ? it->get() : nullptr;
}

FormData* FormForest::GetFormData(const FormGlobalId& form,
                                  FrameData* frame_data) {
  DCHECK(!frame_data || frame_data == GetFrameData(form.frame_token));
  if (!frame_data)
    frame_data = GetFrameData(form.frame_token);
  if (!frame_data)
    return nullptr;
  auto it = base::ranges::find(frame_data->child_forms, form.renderer_id,
                               &FormData::unique_renderer_id);
  return it != frame_data->child_forms.end() ? &*it : nullptr;
}

FormForest::FrameAndForm FormForest::GetRoot(FormGlobalId form) {
  FrameData* frame = nullptr;
  for (;;) {
    frame = GetFrameData(form.frame_token);
    AFCHECK(frame, return {nullptr, nullptr});
    if (!frame->parent_form) {
      auto it = base::ranges::find(frame->child_forms, form.renderer_id,
                                   &FormData::unique_renderer_id);
      AFCHECK(it != frame->child_forms.end(), return {nullptr, nullptr});
      return {frame, &*it};
    }
    form = *frame->parent_form;
  }
}

void FormForest::EraseReferencesTo(
    absl::variant<LocalFrameToken, FormGlobalId> frame_or_form) {
  auto Match = [&](FormGlobalId form) {
    return absl::holds_alternative<LocalFrameToken>(frame_or_form)
               ? absl::get<LocalFrameToken>(frame_or_form) == form.frame_token
               : absl::get<FormGlobalId>(frame_or_form) == form;
  };
  for (std::unique_ptr<FrameData>& some_frame : frame_datas_) {
    AFCHECK(some_frame, continue);
    for (FormData& some_form : some_frame->child_forms) {
      base::EraseIf(some_form.fields, [&](const FormFieldData& some_form) {
        return Match(some_form.renderer_form_id());
      });
    }
    if (some_frame->parent_form && Match(*some_frame->parent_form))
      some_frame->parent_form = absl::nullopt;
  }
}

void FormForest::EraseForm(FormGlobalId form) {
  if (FrameData* frame = GetFrameData(form.frame_token)) {
    base::EraseIf(frame->child_forms, [&](const FormData& some_form) {
      return some_form.global_id() == form;
    });
    EraseReferencesTo(form);
  }
}

void FormForest::EraseFrame(LocalFrameToken frame) {
  some_rfh_for_debugging_ = nullptr;
  if (frame_datas_.erase(frame))
    EraseReferencesTo(frame);
}

// Maintains the following invariants:
// 1. The graph is a disjoint union of trees, and the trees faithfully represent
//    the frame-transcending forms in the DOMs.
// 2. The root forms of each tree contain (only) the nodes of all forms in
//    the subtree in DOM order, and all non-root forms have no fields.
//
// We keep the FormData::child_frame and FrameData::parent_form relations in
// symmetry to ease reasoning about the tree. Since children are predetermined
// by FormData:child_frame, we do so by always updating the childrens'
// FrameData::parent_form.
//
// If |form| is not part of a frame-transcending form, the function has minimal
// overhead. That is, if |form|'s FormData::child_frames is empty and |form|'s
// frame has no parent frame, the function reduces to a few moves and index
// lookups.
//
// If the FrameData::parent_form of |form|'s frame is not set although a parent
// frame exists, the function triggers a reparse in the parent frame. This will
// trigger an UpdateTreeOfRendererForm() for the true parent form (amongst
// others), which will then also set the child frame's FrameData::parent_form.
void FormForest::UpdateTreeOfRendererForm(FormData* form,
                                          ContentAutofillDriver* driver) {
  AFCHECK(form, return );
  AFCHECK(driver, return );
  AFCHECK(form->host_frame, return );
  some_rfh_for_debugging_ = driver->render_frame_host();

  FrameData* frame = GetOrCreateFrameData(form->host_frame);
  AFCHECK(frame, return );
  AFCHECK(!frame->driver || frame->driver == driver, return );
  frame->driver = driver;

  content::RenderFrameHost* rfh = driver->render_frame_host();
  AFCHECK(rfh, return );
  AFCHECK(form->host_frame == LocalFrameToken(rfh->GetFrameToken().value()),
          return );

  // Moves |form| into its |frame|'s FrameData::child_forms, with a special
  // treatment of the fields: |form|'s fields are replaced with |old_form|'s
  // fields if |form| had previously been known as |old_form|.
  //
  // Retaining the old fields is important because |old_form| may have been a
  // root and therefore contained fields from other forms. The relevant old and
  // new fields will be moved to |form|'s root later.
  //
  // Also unsets the FrameData::parent_form pointer for newly removed children.
  // Usually, a removed child frame has been or will be destroyed. However, a
  // child frame may also be removed because the frame became invisible. For
  // simplicity, we do not move fields from |form|'s root back to the former
  // children. Instead, we rely on the descendant frames being reparsed before
  // they become visible again.
  std::vector<FormFieldData> form_fields = std::move(form->fields);
  bool child_frames_changed;
  if (FormData* old_form = GetFormData(form->global_id(), frame)) {
    form->fields = std::move(old_form->fields);
    child_frames_changed = old_form->child_frames != form->child_frames;
    for_each_in_set_difference(
        old_form->child_frames, form->child_frames,
        [this, frame](FrameToken removed_child_token) {
          absl::optional<LocalFrameToken> local_child =
              Resolve(*frame, removed_child_token);
          FrameData* child_frame;
          if (local_child && (child_frame = GetFrameData(*local_child)))
            child_frame->parent_form = absl::nullopt;
        },
        &FrameTokenWithPredecessor::token);
    *old_form = std::move(*form);
    form = old_form;
  } else {
    DCHECK(!base::Contains(frame->child_forms, form->unique_renderer_id,
                           &FormData::unique_renderer_id));
    form->fields = {};
    child_frames_changed = false;
    frame->child_forms.push_back(std::move(*form));
    form = &frame->child_forms.back();
  }
  DCHECK(form && form == GetFormData(form->global_id()));

  // Do *NOT* modify any FrameData::child_forms after this line!
  // Doing so may resize FrameData::child_forms, while we keep raw pointers to
  // individual items.

  // Traverses |form|'s tree starting at its root to set FrameData::parent_form
  // and update the root form's fields, while keeping them in DOM order:
  // - adds the fields from |form_fields| to `root->fields`;
  // - removes fields from `root->fields` that originated from |old_form|'s
  //   renderer form but are not in |form_fields|;
  // - removes fields that originated from frames that have been removed from
  //   |form| as well as fields from their descendant frames;
  // - moves fields from former root forms that are now children of |form|
  //   from that form to `root->fields`.
  //
  // We perform all these tasks in a single pre-order depth-first traversal of
  // |form|'s tree. That is, every field of a form is visited before/after
  // recursing into the subframes of the form that come before/after that field.
  // Only if |form|'s tree is trivial (consists of a single form), we can take
  // an abbreviation and just move the fields.
  if (!frame->parent_form && form->child_frames.empty() &&
      !child_frames_changed) {
    form->fields = std::move(form_fields);
  } else {
    FrameAndForm root = GetRoot(form->global_id());
    AFCHECK(root, return );

    // Moves the first |max_number_of_fields_to_be_moved| fields that originated
    // from the renderer form |source_form| from |source| to |target|.
    // Default-initializes each source field after its move to prevent it from
    // being moved in a future call.
    // Calling MoveFields() repeatedly to move one field at a time runs in
    // quadratic time due to the comparisons of the fields' renderer_form_id().
    // The cost could be reduced to linear time by reversing the source vectors
    // and moving fields from the back of |source|. However, the cost of
    // reversing likely exceeds the cost of the renderer_form_id() comparisons.
    auto MoveFields = [](size_t max_number_of_fields_to_be_moved,
                         FormGlobalId source_form,
                         std::vector<FormFieldData>& source,
                         std::vector<FormFieldData>& target) {
      DCHECK_NE(&source, &target);
      size_t number_of_fields_moved = 0;
      for (FormFieldData& f : source) {
        if (f.renderer_form_id() == source_form) {
          if (++number_of_fields_moved > max_number_of_fields_to_be_moved)
            break;
          target.push_back(std::move(f));
          f = {};  // Clobber |f| in |source| so future MoveFields() skip it.
          DCHECK(!f.renderer_form_id());
        }
      }
    };

    // The |frontier| contains the Nodes to be expanded in LIFO order. Each Node
    // represents the range of fields and the next frame to be visited.
    //
    // A Node consists of
    // - the Node::form whose fields and child frames are to be visited;
    // - the Node::frame that hosts Node::form;
    // - the Node::next_frame to be traversed after visiting the fields that
    //   precede this frame in the form.
    //
    // For example, consider a Node |n| whose form has 4 child frames and at
    // least 5 fields:
    //   <form>
    //     <iframe>
    //     <input id="0">
    //     <input id="1">
    //     <input id="2">
    //     <iframe>
    //     <iframe>
    //     <input id="3">
    //     <input id="4">
    //     <iframe>
    //     <input>
    //     ...
    //   </form>
    // The relative order of fields and frames is encoded in
    // `n.form->child_frames[i].predecessor`: the value represents which field
    // from `n.form`, if any, precedes the frame in DOM order. In this example:
    // - `n.form->child_frames[0].predecessor == -1`;
    // - `n.form->child_frames[1].predecessor == 2`;
    // - `n.form->child_frames[2].predecessor == 2`;
    // - `n.form->child_frames[3].predecessor == 4`.
    // Then the represented field range depends of the next frame:
    // - `n.next_frame == 0` represents the empty range;
    // - `n.next_frame == 1` represents the fields with indices 0, 1, 2;
    // - `n.next_frame == 2` represents the empty range;
    // - `n.next_frame == 3` represents the fields with indices 3, 4;
    // - `n.next_frame == 4` represents the fields with indices greater than 4.
    //
    // Note that the fields of `n.form` are typically not stored in
    // `n.form->fields`:
    // - If `n.form` refers to the passed |form| (`n.form == form`), then these
    //   fields have been moved to |form_fields|.
    // - Otherwise, they are stored in `n.form`'s root form.
    //
    // Since the relative order of the fields is the same as in the renderer
    // form, the index |i| of a field from `n.form` translates to the |i|th
    // field whose `renderer_form_id()` is `n.form->global_id()` in the
    // respective vector that contains the fields from `n.form`.
    //
    // When the traversal expands a Node |n| from the |frontier|, we
    // - pull these fields from |roots_on_path| (defined below) or |form_fields|
    //   and append them to |root_fields|, the future fields of the root;
    // - push on the |frontier|
    //   - one Node for each child form in that frame, and
    //   - a Node with incremented `n.next_frame` for the successor frame and
    //     its preceding fields
    //   in an order that ensures DOM-order traversal.
    // The latter step is omitted if Node::next_frame is out of bounds,
    // indicating indicating that all fields and frames have been visited
    // already.
    struct Node {
      FrameData* frame;   // Not null.
      FormData* form;     // Not null.
      size_t next_frame;  // In the range [0, `form->child_frames.size()`].
    };
    base::stack<Node> frontier;
    frontier.push({root.frame, root.form, 0});

    // Fields to be moved to |root_fields| may not just come from |form_fields|
    // or `root.form->fields`, but also from forms that used to be roots but
    // have become children of |form|. These former roots contain the fields
    // from their subtrees. To access these fields, we store the fields from the
    // root as well as from former root nodes (unless they have no fields) in
    // |roots_on_path|.
    base::stack<FormData*> roots_on_path;

    // New fields of the root form. To be populated in the tree traversal.
    std::vector<FormFieldData> root_fields;
    root_fields.reserve(root.form->fields.size() + form_fields.size());

    size_t num_visits = 0;
    while (!frontier.empty()) {
      // Each node |n| is visited `n.form->child_frames.size() + 1` times.
      // By induction on the tree height, it follows that the overall number of
      // visits is 2 * N - 1, where N is the number of nodes in the tree:
      // For a tree with a single vertex, the claim is immediate. For the
      // induction step, consider a root node with K children. Let N_I be the
      // number of nodes in the subtree induced by the Ith child. By induction,
      // the number of visits in each subtree is 2 * N_I - 1. The number of
      // visits in the whole tree is therefore
      //     K + 1 + \sum_{I=1}^K (2 * N_I - 1)
      //   = 1 + \sum_{I=1}^K 2 * N_I
      //   = 2 * (1 + \sum_{I=1}^K N_I) - 1
      //   = 2 * N - 1.
      if (++num_visits > kMaxParseableFramesInTree * 2 - 1) {
        AFCHECK(false);
        break;
      }

      Node n = frontier.top();
      frontier.pop();
      AFCHECK(n.frame, continue);
      AFCHECK(n.form, continue);

      // Pushes the current form on |roots_on_path| only if this is the first
      // time we encounter the form in the traversal (Node::next_frame == 0).
      if (n.next_frame == 0 && (n.form == root.form || !n.form->fields.empty()))
        roots_on_path.push(n.form);
      AFCHECK(!roots_on_path.empty(), continue);

      std::vector<FormFieldData>& source =
          n.form->global_id() == form->global_id()
              ? form_fields
              : roots_on_path.top()->fields;
      // Moves the next fields from `n.form` to |root_fields|.
      //
      // If `n.next_frame` is in bounds, pushes on the |frontier|
      // - one Node for each child form in that frame, and
      // - a Node with incremented `n.next_frame` for the successor frame and
      //   its preceding fields.
      // The order of these pushes is inversed so that they're expanded in DOM
      // order.
      //
      // If `n.next_frame` is out of bounds, all paths through |n| have been
      // fully traversed, so if applicable, we pop `n.form` from
      // |roots_on_path|.
      if (n.next_frame < n.form->child_frames.size()) {
        // [begin, end) is the range of fields from `n.form` before
        // `n.next_frame`.
        size_t begin = base::checked_cast<size_t>(
            n.next_frame > 0
                ? n.form->child_frames[n.next_frame - 1].predecessor + 1
                : 0);
        size_t end = base::checked_cast<size_t>(
            n.form->child_frames[n.next_frame].predecessor + 1);
        AFCHECK(begin <= end, continue);
        MoveFields(end - begin, n.form->global_id(), source, root_fields);
        frontier.push(
            {.frame = n.frame, .form = n.form, .next_frame = n.next_frame + 1});
        absl::optional<LocalFrameToken> local_child =
            Resolve(*n.frame, n.form->child_frames[n.next_frame].token);
        FrameData* child_frame;
        // If the FrameData does not exist yet, creating it now and setting the
        // FrameData::parent_form avoids a reparse if and when a form is seen in
        // this child frame.
        if (local_child && (child_frame = GetOrCreateFrameData(*local_child))) {
          child_frame->parent_form = n.form->global_id();
          for (size_t i = child_frame->child_forms.size(); i > 0; --i) {
            frontier.push({.frame = child_frame,
                           .form = &child_frame->child_forms[i - 1],
                           .next_frame = 0});
          }
        }
      } else {
        MoveFields(std::numeric_limits<size_t>::max(), n.form->global_id(),
                   source, root_fields);
        if (n.form == roots_on_path.top()) {
          roots_on_path.top()->fields.clear();
          roots_on_path.pop();
        }
      }
    }
    root.form->fields = std::move(root_fields);
  }

  // Triggers a reparse in a parent frame if `frame->parent_form` is unset.
  //
  // If |frame| has a parent frame and is not a fenced frame, there are two
  // scenarios where `frame->parent_form` is unset:
  // - The parent frame hasn't been processed by UpdateTreeOfRendererForm() yet.
  // - The parent form did not include the correct token of |frame| in its
  //   FormData::child_frames (for example, because loading a cross-origin page
  //   into the <iframe> has changed |frame|'s FrameToken).
  //
  // In this case, we trigger a reparse in the parent frame. As a result,
  // UpdateTreeOfRendererForm() will be called for the parent form, whose
  // FormData::child_frames now include |frame|.
  content::RenderFrameHost* parent_rfh = rfh->GetParent();
  if (!frame->parent_form && parent_rfh && !IsFencedFrameRoot(rfh)) {
    LocalFrameToken parent_frame_token(
        LocalFrameToken(parent_rfh->GetFrameToken().value()));
    FrameData* parent_frame = GetFrameData(parent_frame_token);
    if (parent_frame && parent_frame->driver) {
      AFCHECK(parent_frame->driver->render_frame_host() == parent_rfh);
      parent_frame->driver->TriggerReparse();
    }
  }
}

const FormData& FormForest::GetBrowserFormOfRendererForm(
    const FormData& renderer_form) const {
  AFCHECK(renderer_form.host_frame, return renderer_form);

  // For calling non-const-qualified getters.
  FormForest& mutable_this = *const_cast<FormForest*>(this);
  const FormData* form = mutable_this.GetRoot(renderer_form.global_id()).form;
  AFCHECK(form, return renderer_form);
  return *form;
}

std::vector<FormData> FormForest::GetRendererFormsOfBrowserForm(
    const FormData& browser_form,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map)
    const {
  AFCHECK(browser_form.host_frame, return {browser_form});

  // For calling non-const-qualified getters.
  FormForest& mutable_this = *const_cast<FormForest*>(this);

  // Reinstates the fields of |browser_form| in copies of their renderer forms.
  // See the function's documentation in the header for details on the security
  // policy |IsSafeToFill|.
  std::vector<FormData> renderer_forms;
  for (const FormFieldData& browser_field : browser_form.fields) {
    FormGlobalId form_id = browser_field.renderer_form_id();

    // Finds or creates the renderer form from which |browser_field| originated.
    // The form with |form_id| may have been removed from the tree, for example,
    // in between of a refill.
    auto renderer_form =
        base::ranges::find(renderer_forms.rbegin(), renderer_forms.rend(),
                           form_id, &FormData::global_id);
    if (renderer_form == renderer_forms.rend()) {
      const FormData* original_form = mutable_this.GetFormData(form_id);
      if (!original_form)  // The form with |form_id| may have been removed.
        continue;
      renderer_forms.push_back(*original_form);
      renderer_form = renderer_forms.rbegin();
      renderer_form->fields.clear();  // In case |original_form| is a root form.
    }
    DCHECK(renderer_form != renderer_forms.rend());

    auto IsSafeToFill = [&mutable_this, &browser_form, &renderer_form,
                         &triggered_origin,
                         &field_type_map](const FormFieldData& field) {
      // Non-sensitive values may be filled into fields that belong to the main
      // frame's origin. This is independent of the origin of the field that
      // triggered the autofill, |triggered_origin|.
      auto IsSensitiveFieldType = [](ServerFieldType field_type) {
        switch (field_type) {
          case CREDIT_CARD_TYPE:
          case CREDIT_CARD_NAME_FULL:
          case CREDIT_CARD_NAME_FIRST:
          case CREDIT_CARD_NAME_LAST:
            return false;
          default:
            return true;
        }
      };
      // Fields in frames whose permissions policy allows shared-autofill may
      // be filled if the |triggered_origin| is the main origin.
      auto has_shared_autofill_permission = [&mutable_this](
                                                LocalFrameToken frame_token) {
        FrameData* frame = mutable_this.GetFrameData(frame_token);
        return frame && frame->driver && frame->driver->render_frame_host() &&
               frame->driver->render_frame_host()->IsFeatureEnabled(
                   blink::mojom::PermissionsPolicyFeature::kSharedAutofill);
      };

      const url::Origin& main_origin = browser_form.main_frame_origin;
      auto it = field_type_map.find(field.global_id());
      ServerFieldType field_type =
          it != field_type_map.end() ? it->second : UNKNOWN_TYPE;
      return field.origin == triggered_origin ||
             (field.origin == main_origin &&
              !IsSensitiveFieldType(field_type)) ||
             (triggered_origin == main_origin &&
              has_shared_autofill_permission(renderer_form->host_frame));
    };

    renderer_form->fields.push_back(browser_field);
    if (!IsSafeToFill(renderer_form->fields.back()))
      renderer_form->fields.back().value.clear();
  }

  return renderer_forms;
}

}  // namespace internal
}  // namespace autofill
