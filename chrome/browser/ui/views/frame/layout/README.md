# Browser View Layout

This is an overview of the `BrowserViewLayout` system, and a how-to for
adding/modifying layout logic.

Every `BrowserView` has a `BrowserViewLayout`, which is a `LayoutManager` and
which uses the standard Views layout logic. Which implementation is used depends
on the type of browser, and is created by `BrowserViewLayout::CreateLayout()`:
 - Normal browsers:
   [BrowserViewTabbedLayoutImpl](./browser_view_tabbed_layout_impl.h)
 - WebApps:
   [BrowserViewAppLayoutImpl](./browser_view_app_layout_impl.h)
 - Popups:
   [BrowserViewPopupLayoutImpl](./browser_view_popup_layout_impl.h)

`BrowserViewLayout` differs from most `LayoutManager` implementations in that it
lays out not just children of the `BrowserView`, but also descendants and
elements on overlay widgets (e.g. the toolbar and tab strip in immersive
fullscreen mode).

Each layout implementation has access to the following:
 - A `BrowserViewLayoutDelegate` which provides access to layout-specific
   browser and window properties. The layout should never read browser-,
   browser view-, or browser window-specific properties directly from the
   `Browser` or `BrowserView`, so that the behaviors can be mocked for testing.
 - A `BrowserViewLayoutViews` detailing all of the View objects the layout is
   responsible for. When possible, these should be stored as `views::View`s,
   not as specific derived classes, unless some specific layout- or visuals-
   related API needs to be directly accessed by the layout.

## Anatomy of a Browser View Layout

All of the layout work is done in `BrowserViewLayoutImpl::Layout()`.

The steps are as follows:
 1. Get the `BrowserLayoutParams` from the frame by way of the delegate. This
    defines the geometry of the region in which the browser view's contents can
    lay out.
 2. `DoPreLayoutComputations()` - computes and stores any values that are used
    across multiple methods in the layout process. Do not compute anything here
    that is only used in one place.
 3. Lay out the top container if it is not parented to the browser view.
    - Calculate the top container layout using `CalculateTopContainerLayout()`.
      Note that this method is also used when the top container is in the
      browser view and laid out in step (4) below.
    - Apply the top container layout.
    - Optionally position the top container in its parent.
    - Optionally update the working bounds for the rest of the layout.
 4. Calculate and apply the main browser view layout.
    - `CalculateProposedLayout()` is where all of the "normal" layout
    calculation happens. If the top container is in the browser view, this will
    also call `CalculateTopContainerLayout()` at the appropriate time.
 5. `ConfigureTopContainerBackground()` - sets appropriate background properties
    and colors for the top container.
 6. `DoPostLayoutVisualAdjustments()` - sets any visual properties, clip paths,
    fades, corners, etc. that aren't strictly part of layout but which require
    each view to be laid out in its correct bounds.
 7. Update any browser-anchored bubbles using `UpdateBubbles()`.
 8. `DoPostLayoutCleanup()` - clears out any data generated in
    `DoPreLayoutComputations()` or during layout. This is done so that data from
    one layout pass doesn't "leak" into the next, and that transient layout data
    isn't accessed outside of layout.

## Updating/Modifying Browser View Layout

The goal of the `BrowserViewLayout` system is to keep the layout code as simple,
straightforward, and readable as possible. This is a challenge because of the
complexity of `BrowserView`, so great care should be taken in modifying the
code.

### Adding Views to the Layout

To add a new view to the layout, add a field to `BrowserViewLayoutViews` and
ensure it is set in `BrowserView::AddedToWidget()`. Views added in this way
_should_ typically be owned by the BrowserView or part of the top container.

Again, prefer to add a field of type `raw_ptr<views::View>` rather than some
derived class.

### Laying a View Out {#laying-a-view-out}

In `CalculateProposedLayout()` or `CalculateTopContainerLayout()` (depending on
where your View lives), add a block at the appropriate point in the layout:

```cpp
  // In CalculateProposedLayout():
  if (IsParentedTo(views().my_view, views().browser_view)) {
    const bool my_view_should_be_visible = ... // Calculate visibility here.
    gfx::Rect my_view_bounds;
    if (my_view_should_be_visible) {
      my_view_bounds = ... // Calculate view bounds from `params`.
      // Maybe inset `params` based on the space my view will take up.
    }
    // This sets both the bounds and target visibility.
    layout.AddChild(views().my_view, my_view_bounds, my_view_should_be_visible);
  }
```

Note that visibility calculation (if any) and bounds calculation (if any) happen
as part of this block. If you need to "save" the view's visibility or bounds for
some later computation, you can declare them outside the block:

```cpp
  bool my_view_visible = false;
  gfx::Rect my_view_bounds;
  if (IsParentedTo(views().my_view, view().browser_view)) {
    my_view_visible = ...
    my_view_bounds = ...
    layout.AddChild(...);
  }
```

You can also save the layout itself for later use:

```cpp
  ProposedLayout* my_view_layout = nullptr;
  if (IsParentedTo(views().my_view, view().browser_view)) {
    const bool my_view_visible = ...
    gfx::Rect my_view_bounds = ...
    my_view_layout = &layout.AddChild(...);
  }
```

Note that bounds and visibility will be set on the view by the time that
`DoPostLayoutVisualAdjustments()` is called, so you do not need to preserve them
outside of the layout calculation.

If you do need some common value to calculate both your view's bounds and/or
visibility _and_ some visual property that can't be determined directly from
your view's bounds and visibility, the value can be calculated and saved in
`DoPreLayoutComputations()` and then used wherever it is needed.

### Setting the Visibility of a View

Prefer to set the visibility of a view as part of `CalculateProposedLayout()`
(for a full example see [Laying a View Out](#laying-a-view-out) above).

In other words, prefer something like this:

```cpp
  const bool my_view_should_be_visible = ... // Calculate visibility here.
  // ...
  layout.AddChild(views().my_view, my_view_bounds, my_view_should_be_visible);
```

In rare cases, a view's visibility will be set by some external system, in which
case you can do the child layout calculation inside an
`if (IsParentedToAndVisible(...)) { ... }` block.

### Modifying a View's Clip Area, Background, Etc.

All of this work should be done in `DoPostLayoutVisualAdjustments()` if
possible.

For views which use `CustomCornersBackground`, prefer to configure it by
dynamically casting the background rather than adding a custom accessor to the
`View` class (this helps us avoid using specific View classes in
`BrowserViewLayoutViews`):

```cpp
  auto* const my_view_background =
      views().my_view->background()->AsA<CustomCornersBackground>();
  // Set background properties
```

### Adding or Modifying an Animation

See [the BrowserAnimationController documentation](/chrome/browser/ui/animation/README.md)
for information on how to create and extend animations.

Some general guidelines:
 - If the parameter only affects e.g. the preferred size of a `View`, you can
   read it in that View's `CalculatePreferredSize()` method and not reference it
   directly in the layout at all.
 - If the specific parameter only affects visuals, you can read and use it in
   `DoPostLayoutVisualAdjustments()`.
 - If the parameter only affects the geometry of a single element and does not,
   for example, affect the horizontal layout of the browser window, you can read
   and use it directly in `CalculateProposedLayout()`.
 - Otherwise, you should read and store it (or some value derived from it) in
   `DoPreLayoutComputations()` so it can be used throughout the layout.

You may need to have your view listen for animation updates from the
`BrowserAnimationController` just to call `InvalidateLayout()` or
`PreferredSizeChanged()`.

### General Guidelines

 * Do not set the bounds of a view managed by a `BrowserViewLayout` outside of
   `BrowserViewLayout::CalculateProposedLayout()`.
 * Try not to set the visibility of a view managed by a `BrowserViewLayout`
   outside of `BrowserViewLayout::CalculateProposedLayout()`.
 * Try not to configure any foreground or background visual properties of a view
   managed by a `BrowserViewLayout` outside of
   `BrowserViewLayout::DoPostLayoutVisualAdjustments()`.

Initial states may, however, be set during `View` creation.
