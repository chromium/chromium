The **ExecutionContextPriorityDecorator** is responsible for assigning the
priority of all the frames and workers in the graph.

This is done through a system of voting where each voter can increase the
priority of an execution context independently, and the vote with the highest
priority determines which priority the given frame or worker will be assigned.

The **RootVoteObserver** is a simple layer on top of the voting system that
receives the final vote for an execution context and does the actual assignment
to the graph node.

Each voter tracks a single property of an execution context and casts their vote
via their voting channel to the **MaxVoteAggregator**. An example is the
**FrameAudibleVoter** class.

It is possible to cast a "negative" vote which can reduce the priority of an
execution context by overriding the vote of all the other voters. An example is
the **AdFrameVoter** class.

In addition, there is a **BoostingVoteAggregator**, which introduces the notion
of BoostingVotes. They ensure that an execution context's priority is always
equal or higher than the priority of another execution context, by casting a
vote with a matching priority. An example is the **ChildFrameBooster** class.
