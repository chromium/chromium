# Browser Window Explainer

Each browser window has the same structure, depicted below:

- [BrowserWidget](./browser_widget.h) - The widget hosting the browser window.
  This should only provide general widget-y information.
  - [BrowserNativeWidget](./browser_native_widget.h) - Provides
    platform-specific browser behavior. Every implementation of this interface
    is also a `NativeWidget`.
  - [NonClientView](//ui/views/window/non_client_view.h) - Standard non-client
    view for processing events, etc.
    - [BrowserFrameView](./browser_frame_view.h) - Provides information about
      the frame around the browser content, including border and layout
      information, caption buttons, window icon, title, etc. Implementation may
      be dependent on both browser type and platform.
      - [BrowserView](./browser_view.h) - The browser itself.
        - [BrowserViewLayout](./browser_view_layout.h) - Responsible for laying
          out the browser. Only knows about `BrowserViewLayoutDelegate`.
          - [BrowserViewLayoutDelegate](./browser_view_layout_delegate.h) -
            Conduit through which the `BrowserViewLayout` retrieves information
            about the layout. Implemented by `BrowserView`; designed to be able
            to be mocked.

### Diagram

```
┌─────────────────────────────────────┐   ┌───────────────────────┐
│ BrowserWidget                       │ ↔ │ BrowserNativeWidget + │
│ ┌─────────────────────────────────┐ │   │ NativeWidget          │
│ │ RootView                        │ │   └───────────────────────┘
│ │ ┌─────────────────────────────┐ │ │
│ │ │ NonClientView               │ │ │
│ │ │ ┌─────────────────────────┐ │ │ │
│ │ │ │ BrowserFrameView        | │ │ │
│ │ │ │ ┌─────────────────────┐ │ │ │ │
│ │ │ │ │ BrowserView         │ │ │ │ │
│ │ │ │ │ (ClientView)        │ │ │ │ │
│ │ │ │ │                     │ │ │ │ │
│ │ │ │ └─────────────────────┘ │ │ │ │
│ │ │ └─────────────────────────┘ │ │ │
│ │ └─────────────────────────────┘ │ │
│ └─────────────────────────────────┘ │
└─────────────────────────────────────┘
```

## Usage and Implementation Notes

### Where do specific functions go?

If you're not sure where some piece of functionality goes:
 - Anything relating to the content of the browser window goes in `BrowserView`.
   - Unless it's general layout-related logic, in which case it goes in
     `BrowserViewLayout`.

 - Anything relating to the frame and titlebar of the browser window goes in
   `BrowserFrameView`, including but not limited to:
   - Border
   - Shadow
   - App icon
   - Title
   - Caption buttons
   - Where the client area or can and cannot lay itself out
   - Hit-testing
   - Platform-specific information relating to any of the above (implemented in
     platform-specific subclasses)

 - Any information or features of the whole window that are platform-specific go
   in `BrowserNativeWidget` and are implemented in its platform-specific
   subclasses.

 - Any `views::Widget`-specific stuff, like window event handling, goes in
   `BrowserWidget`
   - But note that platform-specific logic can be handed off to `BrowserNativeWidget`.

### Platform-agnostic vs. platform-specific logic

Cross-desktop-platform code should have access to `BrowserView`,
`BrowserWidget`, `BrowserFrameView`, and `BrowserNativeWidget`. Therefore it is
not necessary to pipe calls through another class if the class that actually has
the method in question is available.

(The one exception is `BrowserViewLayout`, which has access only to
`BrowserViewLayoutDelegate`, so calls must be piped through that class.)

Platform-specific logic should be behind a platform-agnostic API when possible,
and if it actually applies to multiple platforms. If there is a logic path that
only applies to one platform, try to encapsulate it in a platform-specific
implementation; if it absolutely must be referenced outside of that
implementation, place any required references in `#if BUILDFLAG()` blocks.

For example, `BrowserFrameView::CaptionButtonsOnLeadingEdge()` is a general
question one might ask about any browser frame - "are the caption buttons on
the leading (vs. trailing) edge of the window?" since caption buttons are
present on every desktop platform, _and it is used in cross-platform code_.
Therefore it goes in `BrowserFrameView`, and should be called directly if that
information is needed.

On the other hand, `GetMinimizeButtonOffset()` lives in `BrowserFrameViewWin`
not because it's a question that could be asked about any browser, but is only
ever actually used on Windows. It's an implementation detail that doesn't need
to be in the general interface.
