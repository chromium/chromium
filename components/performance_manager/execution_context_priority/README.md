The **PriorityVotingSystem** is responsible for assigning the
priority of the votable nodes in the graph.

This is done through a system of voting where each voter can independently
increase the priority of a graph node, and the vote with the highest
priority determines which priority that graph node will be assigned.

A **VoteContext** identifies the graph node that a vote targets. It is a
`std::variant` that explicitly lists the votable node types (currently
`FrameNode`, `WorkerNode` and `ProcessNode`). Voting on any other node type
fails to compile.

## Vote flow

```
                VotingChannel                       MaxVoteAggregator::Observer
 Voters --------------------+
                            +--> MaxVoteAggregator -----> PrioritySetter: frames, workers, processes
 ProcessPriorityAggregator -+
```

Every vote goes into a single **MaxVoteAggregator**, through a voting channel.
The top vote of a node in the aggregator is therefore its final priority.

Each voter tracks a single property of a vote context and casts their vote
via their voting channel to the MaxVoteAggregator.

The **PrioritySetter** observes the MaxVoteAggregator, and assigns the top vote
of frames, workers and (renderer) processes to their priority, or their default
priority when they have no vote.

The **ProcessPriorityAggregator** follows the priority of every frame and worker
(as set by the PrioritySetter), and casts the highest priority of the frames
and workers hosted by a process as a vote on that process, where it is
aggregated with the votes cast directly on the process. This vote is cast even
when it is the lowest priority, so that a process drops from its initial high
priority once it hosts a frame or worker.

The **PriorityVotingSystem** owns the MaxVoteAggregator, and builds each of
these from it. It also verifies that no votes are leaked.

## Invariants

- **No cycles:** Derived votes only flow from frames and workers to processes.
  Processes never cast votes. The MaxVoteAggregator CHECKs that the top vote
  for a node doesn't change while its observers are being notified, which
  catches any cycle.
- **Re-entrancy:** Votes can be cast while the MaxVoteAggregator is notifying
  its observers, both by voters (e.g. the InheritParentPriorityVoter votes on
  child frames when the PrioritySetter sets the priority of a frame) and by the
  ProcessPriorityAggregator (when the priority of a frame or worker changes), so
  the aggregator supports being re-entered.
- **Timing:** The PriorityVotingSystem must be added to the graph before any
  node (this is CHECKed), so that the ProcessPriorityAggregator sees every frame
  and worker being added. The priority of a frame or worker is taken into
  account in the vote on its process once it is added to the graph, and is
  followed until it is removed.
- **No leaks:** Voters must remove their votes on a node before it is removed
  from the graph (this is CHECKed).

## Voters vs. derived votes

Some voters (e.g. **InheritClientPriorityVoter**) also derive votes from the
priority of other nodes. They are voters because they implement an optional
policy. The derived votes cast on processes (from their frames and workers) are
part of the voting system itself: they are always needed for the priorities to
be correct.
