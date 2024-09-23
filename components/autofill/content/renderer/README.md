# Architecture

- `form_autofill_util.{cc,h}` contains the functions for extracting and
  manipulating DOM elements.
- `AutofillAgent` is instantiated per `content::RenderFrame`.
- `AutofillAgent` owns `PasswordAutofillAgent` and
  `PasswortGenerationAgent`.

# Terminology

To understand form extraction, a bit of terminology is crucial.
The most important concept is ownership.

## Form controls

There are multiple categories of form-related DOM elements:

- *Form control* elements are `input`, `textarea`, `select`, `button`,
  `fieldset`, `output` elements.
- *Autofillable* form control elements are `input` (certain types), `textarea`,
  `select` (certain types) elements.
- *Listed* elements are the form control elements plus `object` and
  form-associated custom elements.
- *Form-associated* elements are the listed control elements plus `img`
  elements.

For Autofill, only the first two catogeries matter.

Autofill does not currently support [form-associated custom elements].

See [this slide](https://goto.corp.google.com/autofill-form-control-categories)
for a graphical overview.

## Association

A form control element `t` (e.g., an `<input>`) can be *associated* with a form
element inside its DOM. Examples include

- `<form id=f><input id=t></form>`
- `<form id=f></form><input id=t form=f>`

In both examples, the form control element `t` is associated with the form
element `f`.

Form association is an HTML concept independent of Autofill. We refer to
the spec section about form-[associated] elements for more detail.

## Top-most form elements

A form element is called a *top-most* form iff it has no [shadow-including]
form element ancestor.

See [this README](/third_party/blink/renderer/core/dom/README.md) for further
details on DOM traversals.

## Ownership

Ownership is an Autofill concept. Its objective is to go beyond HTML's form
association in the following ways:

- It transcends [node tree]s: for example, a `form` element in the [document
  tree] owns all form controls in [shadow tree]s hosted by descendants of the
  `form` element;
- It gathers unowned form controls: we treat the collection of unowned form
  controls just like another separate form;
- It extends to contenteditables: we treat a contenteditable like a form with a
  single field.

A form control element `t` is *owned* by a top-most form element `f` iff
`t` is [connected] and

- `t` is [associated] with `f` or a descendant of `f`, or
- `t` is a [shadow-including] descendant of `f` and `t` and `f` are not in the
  same [node tree].

Note that allowing `t` to be [associated] with a descendant of `f` instead of
`f` accommodates unconforming (but possible) scenarios in which there are
nested forms within the same DOM tree. In that case, `t` may be associated with
any form, but we want its *owning* form to always be a top-level form.

A form control element `t` is *unowned* iff `t` is [connected] and no top-most
form element owns `t`. That is, to be explicit, `t` is unowned iff `t` is
[connected] and

- `t` is not [associated] with any form element or
- `t` has no [shadow-including] form element ancestor in another [node tree].

We refer to the collection of unowned form controls as the *unowned form* and, in
a slight abuse of terminology, say that the unowned form *owns* the unowned form
controls. The unowned form is represented by the null `WebFormElement`.

A [contenteditable] is *owned* by itself iff it is [connected], not a form
element, not a form control element, and its parent is not [editable].

Ownership determines the relationship between `FormData` objects (representing a
top-most form, a synthetic form for contenteditables, or the unowned form) and
`FormFieldData` objects (representing an autofillable form control element or a
contenteditable).

Note: The term [form owner] used in the HTML spec about form-[associated]
elements is unrelated to Autofill's concept of ownership.

**WARNING:** Autofill code shall only call `GetOwningForm()` and
`GetOwnedFormControls()` to determine the owner/ownee relationship between forms
and form controls. A presubmit script warns when code uses the Blink-analogues
to these functions.

[connected]: https://dom.spec.whatwg.org/#connected
[associated]: https://html.spec.whatwg.org/#association-of-controls-and-forms
[form owner]: https://html.spec.whatwg.org/#form-owner
[shadow-including]: https://dom.spec.whatwg.org/#concept-shadow-including-descendant
[node tree]: https://dom.spec.whatwg.org/#concept-node-tree
[document tree]: https://dom.spec.whatwg.org/#document-trees
[shadow tree]: https://dom.spec.whatwg.org/#shadow-trees
[contenteditable]: https://html.spec.whatwg.org/index.html#attr-contenteditable
[editable]: https://w3c.github.io/editing/docs/execCommand/#editable
[shadow DOM]: https://developer.mozilla.org/en-US/docs/Web/API/Web_components/Using_shadow_DOM
[form-associated custom elements]: https://web.dev/articles/more-capable-form-controls
