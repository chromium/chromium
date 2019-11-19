# Performance Manager Overview

The Performance Manager centralizes policy for data-driven resource management
and planning. Central to this is the [graph](graph/graph_impl.h) which is
comprised of nodes that reflect the coarse structure of a browser at the
level of [pages](graph/page_node_impl.h), [frames](graph/frame_node_impl.h)
[processes](graph/process_node_impl.h) and so forth. Each node is adorned with
relationships and properties sufficient to allow performance management policy
to reason about such things as resource usage and prioritization.
