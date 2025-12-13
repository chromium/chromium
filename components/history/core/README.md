## Core History Component

Code in components/history/core is cross-platform. The core/ directory defines
many of the databases that make up the HistoryDatabase (URLDB, DownloadDB,
VisitDB, etc.), although not all of them. Additionally, it contains the core
functionality behind the three history services described in
components/history/README.md. Finally, core/ also houses sync/ which contains
code to bridge a user's local and remote history.

## Do you have an ios-simulator trybot error?
If you have a CL where all the trybots are passing EXCEPT ios-simulator, it
may be due to layering issues!

A typical error here may look like an issue with DEPS or BUILD files:
```
ERROR at //third_party/blink/public/mojom/BUILD.gn:1628:26: Assignment had no effect.
  component_deps_blink = [...]
```

However, the issue is deeper. Since components/history/core is cross-platform,
it is used when building chromium on iOS. As a result, components/history/core
CANNOT have any dependencies on third_party/blink code.

To fix your CL, you may need to stop using a blink::feature, pass information
via a different struct or enum, or reconsider where your code lives if the
class should not be used on iOS.

## Do you have another layering issue?
We must also note that the DEPS for components/history/core explicitly FORBID
dependencies on [content or components/history/content code](https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/DEPS).

If you're having issues with content/ dependencies from code in components,
try factoring that code out into its own core/ subdirectory, free from any
content/ deps [similar to this :visited link code](https://source.chromium.org/chromium/chromium/src/+/main:components/visitedlink/core/).
