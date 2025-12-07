The sensitive content component is responsible for detecting form content that
should be marked as "sensitive" on Android V+ (e.g., payment information, login
information). It is used by both Chrome and WebView on Android.

If a view is marked as sensitive, and the API level is at least 35, the OS will
redact the view during screen sharing, screen recording, and similar actions.

The `SensitiveContentClient` is used for dependency injection from the embedder.
The client communicates with the embedder, in order to mark/unmark the content
as being sensitive.

The `SensitiveContentManager` is owned by the client. It contains
platform-independent logic and tracks whether sensitive fields are present or
not.
