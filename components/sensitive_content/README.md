The sensitive content component is responsible for detecting form content that
should be marked as "sensitive" on Android V+ (e.g., payment information, login
information). It is used by both Chrome and WebView on Android.

If a view is marked as sensitive, and the API level is at least 35, the OS will
redact the view during screen sharing, screen recording, and similar actions.

The detection is based on Autofill's field type detection. An example field type
is `CREDIT_CARD_NUMBER`. These predictions do not come with a correctness
guarantee, and they may change over time. In particular:

- A document directly influences the types of its own fields through DOM
  elements like `<label>` and attributes like `autocomplete`, `id`, and `name`.
- A document may, intentionally or not, influence field types in other frames
  of the same page. For examples, see
  [crbug.com/513309464#comment8](https://crbug.com/513309464#comment8).

The `SensitiveContentClient` is used for dependency injection from the embedder.
The client communicates with the embedder, in order to mark/unmark the content
as being sensitive.

The `SensitiveContentManager` is owned by the client. It contains
platform-independent logic and tracks whether sensitive fields are present or
not.
