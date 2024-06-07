// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_FOREST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_FOREST_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill::internal {

// FormForest converts renderer forms into a browser form and vice versa.
//
// A *frame-transcending* form is a form whose logical fields live in different
// frames. The *renderer forms* are the FormData objects as we receive them from
// the renderers of these frames. The *browser form* of a frame-transcending
// form is its root FormData, with all fields of its descendant FormDatas moved
// into the root.
// See AutofillDriverRouter for further details on the terminology and
// motivation.
//
// Consider the following main frame with two frame-transcending forms:
//
//                    +--------------- Frame ---------------+
//                    |                                     |
//             +--- Form-A --+                       +--- Form-C --+
//             |      |      |                       |             |
//          Field-1 Frame Field-4                  Frame         Frame
//                    |                              |             |
//             +--- Form-B --+                     Form-D        Form-E
//             |             |                       |             |
//          Field-2       Field-3                 Field-5        Frame
//                                                                 |
//                                                               Form-F
//                                                                 |
//                                                              Field-6
//
// The renderer forms are form A, B, C, D, E, F.
//
// The browser form of forms A and B has the fields fields 1, 2, 3, 4.
// Converting this browser form back to renderer forms yields Form-A and Form-B.
//
// Analogously, the browser form of forms C, D, E, and F has the fields 5 and 6.
// Converting this browser form back to renderer forms yields forms C, D, E, F.
//
// The three key functions of FormForest are:
// - UpdateTreeOfRendererForm()
// - GetBrowserForm()
// - GetRendererFormsOfBrowserFields()
//
// UpdateTreeOfRendererForm() incrementally builds up a graph of frames, forms,
// and fields.
//
// This graph is a forest in two (entirely independent) ways:
//
// Firstly, there may be multiple root frames. One reason is the website author
// can disconnect an entire frame subtree from the rest of the frame tree in the
// future using the fencedframes tag and/or disallowdocumentaccess attribute.
// Another reason is that the frame hierarchy emerges gradually and therefore
// some links may be unknown. For example, Form-A might point to a nonexistent
// frame of Form-B because, after Form-A was last parsed, a cross-origin
// navigation happened in Form-B's frame.
//
// Secondly, removing the root frames obtains a forest, where each tree
// corresponds to a frame-transcending form. We call the roots of this forest
// *root forms*. In the example, forms A and C are root forms. This is relevant
// because filling operations happen on the granularity of root forms.
//
// As an invariant, UpdateTreeOfRendererForm() keeps each frame-transcending
// form in a flattened state: fields are stored as children of their root
// forms. The fields are ordered according to pre-order depth-first (DOM order)
// traversal of the original tree. In our example:
//
//                    +--------------- Frame ---------------+
//                    |                                     |
//    +-------+---- Form-A ---+-------+        +-------+- Form-C -+-------+
//    |       |       |       |       |        |       |          |       |
// Field-1 Field-2 Field-3 Field-4  Frame   Field-5 Field-6     Frame   Frame
//                                    |                           |       |
//                                  Form-B                      Form-D  Form-E
//                                                                        |
//                                                                      Frame
//                                                                        |
//                                                                      Form-F
//
// There is no meaningful order between the fields and frames in these flattened
// forms.
//
// GetBrowserForm(renderer_form) simply retrieves the form node of
// |renderer_form| and returns the root form, along with its field children. For
// example, if |renderer_form| is form B, it returns form A with fields 1–4.
//
// GetRendererFormsOfBrowserFields(browser_fields) returns the individual
// renderer forms that constitute `browser_fields`, with their fields
// reinstated. For example, if `browser_fields` has fields 1–4, it returns form
// A with fields 1 and 4, and form B with fields 2 and 3.
//
// The node types in the forest always alternate as follows:
// - The root nodes are frames.
// - The children of frames are forms.
// - The children of forms are frames or fields.
// - Fields are leaves. Forms and frames may be leaves.
//
// Frames, forms, and fields are represented as FrameData, FormData, and
// FormFieldData objects respectively. The graph is stored as follows:
// - All FrameData nodes are stored directly in FormForest::frame_datas_.
// - The FrameData -> FormData edges are stored in the FrameData::child_forms
//   vector of FormData objects. That is, each FrameData directly holds its
//   children.
// - The FormData -> FrameData edges are stored in the FormData::child_frames
//   vector of FrameTokens. To retrieve the actual FrameData child, the token
//   is resolved to a LocalFrameToken if necessary (see Resolve()), and then
//   looked up in FormForest::frame_datas_.
// - The FormData -> FormFieldData edges are stored in the FormData::fields
//   vector of FormFieldData objects. As per the aforementioned invariant,
//   fields are only stored in root forms. Each field's original parent can be
//   identified by FormFieldData::host_frame and FormFieldData::host_form_id.
//
// Reasonable usage of FormForest follows this protocol:
// 1. Call UpdateTreeOfRendererForm(renderer_form) whenever a renderer form is
//    seen to make FormForest aware of the (new or potentially changed) form.
// 2. Call GetBrowserForm(renderer_form.global_id()) directly afterwards (as
//    long as the renderer form is known to the FormForest).
// 3. Call GetRendererFormsOfBrowserFields(browser_fields) only if
//    `browser_fields` was previously returned by GetBrowserForm(), perhaps with
//    different FormFieldData::value, FormFieldData::is_autofilled.
//
// For FormForest to be memory safe,
// 1. UpdateTreeOfRendererForm() and GetRendererFormsOfBrowserFields() must only
//    be called for forms which have the following attributes set:
//    - FormData::host_frame
//    - FormData::renderer_id
//    - FormFieldData::host_frame
//    - FormFieldData::renderer_id
//    - FormFieldData::host_form_id
// 2. GetBrowserForm() must only be called for known renderer forms. A renderer
//    form is *known* after a corresponding UpdateTreeOfRendererForm() call
//    until it is erased by EraseForms() or EraseFormsOfFrame().
//
// FormForest works with LocalFrameToken and resolves the RemoteFrameTokens in
// FormData::child_frames to LocalFrameTokens.
//
// From the perspective of a frame F, a frame G is either local or remote:
// - If G is local, G is hosted by the same render process as F.
// - If G is remote, G may be hosted by another render process.
//
// Suppose F is the parent frame of G. If G is local to F, then F refers to G in
// its FormData::child_frames by G's LocalFrameToken. Otherwise, if G is remote
// to F, then F uses a RemoteFrameToken as a placeholder to refer to G in
// FormData::child_frames.
//
// While LocalFrameTokens are unique identifiers at any point in time, they may
// change when a navigation happens in the frame:
// - If G is local to F and a navigation causes G's render process to be
//   swapped so that G becomes remote, G gets a new LocalFrameToken and F will
//   refer to G by a fresh RemoteFrameToken.
// - If G is remote to F and a navigation causes G's render process to be
//   swapped, then F may continue to refer to G by the same RemoteFrameToken
//   as before even if G's LocalFrameToken has changed.
// The first example is the reason why UpdateTreeOfRendererForm() sometimes
// triggers form re-extraction in a parent frame. The second example is the
// reason why we do not cache LocalFrameTokens.
class FormForest {
 public:
  // A FrameData is a frame node in the form tree. Its children are FormData
  // objects.
  struct FrameData {
    // Less-than relation on FrameData objects based on their frame token, to be
    // used by FrameData sets.
    struct CompareByFrameToken {
      using is_transparent = void;
      bool operator()(const std::unique_ptr<FrameData>& f,
                      const std::unique_ptr<FrameData>& g) const {
        return f && g ? f->frame_token < g->frame_token : f.get() < g.get();
      }
      bool operator()(const std::unique_ptr<FrameData>& f,
                      const LocalFrameToken& g) const {
        return f ? f->frame_token < g : true;
      }
      bool operator()(const LocalFrameToken& f,
                      const std::unique_ptr<FrameData>& g) const {
        return g ? f < g->frame_token : false;
      }
    };

    explicit FrameData(LocalFrameToken frame_token);
    FrameData(const FrameData&) = delete;
    FrameData& operator=(const FrameData&) = delete;
    ~FrameData();

    // Unique identifier of the frame. This is never null.
    const LocalFrameToken frame_token;
    // List of forms directly contained in the frame, in the same order as the
    // corresponding UpdateTreeOfRendererForm(child_form) calls. Modification of
    // this vector should be kept to a minimum to ensure memory safety. Only
    // UpdateTreeOfRendererForm() should modify it.
    std::vector<FormData> child_forms;
    // Unique identifier of the form in the parent frame that contains this
    // frame. When a parent form can Resolve() a child's FrameToken, it sets
    // itself as the parent of the child frame, even if no form in this frame
    // has been seen yet.
    std::optional<FormGlobalId> parent_form = std::nullopt;
    // Pointer to the frame's AutofillDriver. This may be null because an
    // empty FrameData is created when a parent form can Resolve() a child's
    // LocalFrameToken and no form from that child frame has been seen yet.
    // However, if |child_forms| is non-empty, then driver is non-null.
    raw_ptr<AutofillDriver> driver = nullptr;
  };

  FormForest();
  FormForest(const FormForest&) = delete;
  FormForest& operator=(const FormForest&&) = delete;
  ~FormForest();

  // Adds or updates |renderer_form| and |driver| to/in the relevant tree, where
  // |driver| must be the AutofillDriver of `renderer_form.host_frame`.
  // Afterwards, `renderer_form.global_id()` is a known renderer form.
  void UpdateTreeOfRendererForm(FormData renderer_form,
                                AutofillDriver& driver) {
    UpdateTreeOfRendererForm(&renderer_form, driver);
  }

  // Returns the browser form of a known |renderer_form|.
  const FormData& GetBrowserForm(FormGlobalId renderer_form) const;

  // Parameter type of GetRendererFormsOfBrowserFields().
  class SecurityOptions {
   public:
    // Dangerous: only use this if you know what you're doing.
    // See GetRendererFormsOfBrowserFields() to understand the consequences.
    static constexpr SecurityOptions TrustAllOrigins() { return {}; }

    SecurityOptions(
        const url::Origin* main_origin,
        const url::Origin* triggered_origin,
        const base::flat_map<FieldGlobalId, FieldType>* field_type_map);

    bool all_origins_are_trusted() const { return !main_origin_; }
    const url::Origin& main_origin() const { return *main_origin_; }
    const url::Origin& triggered_origin() const { return *triggered_origin_; }
    FieldType GetFieldType(const FieldGlobalId& field) const;

   private:
    constexpr SecurityOptions() = default;

    // The main frame's origin. If null, all (!) origins are trusted.
    const raw_ptr<const url::Origin> main_origin_ = nullptr;
    // The origin of the field from which Autofill was queried.
    const raw_ptr<const url::Origin> triggered_origin_ = nullptr;
    // Contains the field types of the fields in the browser form.
    const raw_ptr<const base::flat_map<FieldGlobalId, FieldType>>
        field_type_map_ = nullptr;
  };

  // Return type of GetRendererFormsOfBrowserFields().
  struct RendererForms {
    RendererForms();
    RendererForms(RendererForms&&);
    RendererForms& operator=(RendererForms&&);
    ~RendererForms();
    std::vector<FormData> renderer_forms;
    base::flat_set<FieldGlobalId> safe_fields;
  };

  // Returns the renderer forms that host the `browser_fields`. The field values
  // are subject to the security policy for cross-frame previewing and filling.
  //
  // The function reinstates each of the `browser_fields` in the renderer form
  // it originates from. These reinstated fields have the (possibly autofilled)
  // value from `browser_fields`, provided that they are considered safe to fill
  // according to the security policy defined below. The FormFieldData::value
  // of unsafe fields is reset to the empty string.
  //
  // A field is *safe to fill* iff at least one of the conditions (1–4) and
  // additionally condition (5) hold:
  //
  // (1) All origins are trusted (that's dangerous!).
  // (2) The field's origin is the triggered origin.
  // (3) The field's origin is the main origin, the field's type is
  //     non-sensitive, and the policy-controlled feature shared-autofill is
  //     enabled in the field's frame.
  // (4) The triggered origin is the main origin and the policy-controlled
  //     feature shared-autofill is enabled in the field's frame.
  // (5) The field is in the same frame tree as the field on which Autofill was
  //     triggered.
  //
  // All origins are trusted iff `security_options.all_origins_are_trusted()`.
  //
  // The *origin of a field* is the origin of the frame that contains the
  // corresponding form-control element.
  //
  // The *triggered origin* is the origin of the field from which Autofill was
  // queried, `security_options.triggered_origin()`.
  //
  // The *main origin* is the origin of the main frame of the frame of the field
  // from which Autofill was queried, `security_options.main_origin()`.
  //
  // A *type of a field* is determined by `security_options.GetFieldType()`. The
  // non-sensitive field types are credit card types, cardholder names, and
  // expiration dates.
  //
  // The "allow" attribute of the <iframe> element controls whether the
  // *policy-controlled feature shared-autofill* is enabled in a document
  // (see https://www.w3.org/TR/permissions-policy-1/).
  RendererForms GetRendererFormsOfBrowserFields(
      base::span<const FormFieldData> browser_fields,
      const SecurityOptions& security_options) const;

  // Deletes all forms and fields that originate from the |renderer_forms| and
  // unsets the FrameData::parent_form pointers of all child forms.
  //
  // Afterwards, the |renderer_forms| are unknown.
  //
  // Returns the forms that lost fields due to the removal, which are known
  // renderer forms.
  base::flat_set<FormGlobalId> EraseForms(
      base::span<const FormGlobalId> renderer_forms);

  // Deletes all forms and fields that originate from |frame| and unsets the
  // FrameData::parent_form pointers of all child forms.
  //
  // Afterwards, all renderer forms in |frame| are unknown.
  void EraseFormsOfFrame(LocalFrameToken frame, bool keep_frame);

  // Returns the set of FrameData nodes of the forest.
  const base::flat_set<std::unique_ptr<FrameData>,
                       FrameData::CompareByFrameToken>&
  frame_datas() const {
    return frame_datas_;
  }

 private:
  friend class FormForestTestApi;

  struct FrameAndForm {
    raw_ref<FrameData> frame;
    raw_ref<FormData> form;
  };

  // Returns the FrameData known for |frame|, or creates a new one and returns
  // it, in which case all members but FrameData::host_frame are uninitialized.
  FrameData* GetOrCreateFrameData(LocalFrameToken frame);

  // Returns the FrameData known for |frame|, or null.
  // May be used in const qualified methods if the return value is not mutated.
  FrameData* GetFrameData(LocalFrameToken frame);

  // Returns the FormData known for |form|, or null.
  // The returned value will point to |frame_datas_|, meaning that all fields
  // have been moved to their respective root forms.
  // The |frame_data| must be null or equal to `GetFrameData(form.host_frame)`.
  // Beware of invalidating the returned form pointer by changing its host
  // frame's FrameData::host_forms.
  // May be used in const qualified methods if the return value is not mutated.
  FormData* GetFormData(FormGlobalId form, FrameData* frame_data = nullptr);

  // Returns the non-null root frame and form of the tree that contains |form|.
  // Beware of invalidating the returned form pointer by changing its host
  // frame's FrameData::host_forms.
  // May be used in const qualified methods if the return value is not mutated.
  FrameAndForm GetRoot(FormGlobalId form);

  // Helper for EraseFormsOfFrame() and EraseForms() that removes all fields
  // that originate from |frame_or_form| and unsets FrameData::parent_form
  // pointer of |frame_or_form|'s children.
  //
  // Afterwards, all renderer forms in |frame_or_form| (if it is a frame) or the
  // renderer form |frame_or_form| (if it is a form) are unknown.
  //
  // Adds every known renderer form from which a field is removed is to
  // |forms_with_removed_fields|.
  //
  // We intentionally iterate over all frames and forms to search for fields
  // from |frame_or_form|. Alternatively, we could limit this to the root form
  // of |frame_or_form|. However, this would rely on |frame_or_form| being
  // erased before its ancestors, since otherwise |frame_or_form| is
  // disconnected from its root already.
  void EraseReferencesTo(
      absl::variant<LocalFrameToken, FormGlobalId> frame_or_form,
      base::flat_set<FormGlobalId>* forms_with_removed_fields);

  // Adds |renderer_form| and |driver| to the relevant tree, where |driver| must
  // be the AutofillDriver of the |renderer_form|'s FormData::host_frame.
  //
  // Afterwards, `renderer_form->global_id()` is a known renderer form.
  //
  // Leaves `*renderer_form` in a valid but unspecified state (like after a
  // move). In particular, `*renderer_form` and its members can be reassigned.
  void UpdateTreeOfRendererForm(FormData* renderer_form,
                                AutofillDriver& driver);

  // The FrameData nodes of the forest.
  // Note that since the elements are (smart) pointers, they are not invalidated
  // when the set is resized (unlike pointers or references to the elements).
  base::flat_set<std::unique_ptr<FrameData>, FrameData::CompareByFrameToken>
      frame_datas_;
};

}  // namespace autofill::internal

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_FOREST_H_
