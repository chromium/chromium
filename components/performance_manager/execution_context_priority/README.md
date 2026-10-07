The **PriorityVotingSystem** is responsible for assigning the
priority of the votable nodes in the graph.

This is done through a system of voting where each voter can independently
increase the priority of a graph node, and the vote with the highest
priority determines which priority that graph node will be assigned.

A **VoteContext** identifies the graph node that a vote targets. It is a
`std::variant` that explicitly lists the votable node types (currently
`FrameNode` and `WorkerNode`). Voting on any other node type fails to compile.

## Vote flow

```
          VotingChannel                       MaxVoteAggregator::Observer
 Voters ---------------> MaxVoteAggregator -----> PrioritySetter: frames, workers
```

Every vote goes into a single **MaxVoteAggregator**, through a voting channel.
The top vote of a node in the aggregator is therefore its final priority.

Each voter tracks a single property of a vote context and casts their vote
via their voting channel to the MaxVoteAggregator.

The **PrioritySetter** observes the MaxVoteAggregator, and assigns the top vote
of frames and workers to their priority, or their default priority when they
have no vote.

The **PriorityVotingSystem** owns the MaxVoteAggregator, and builds the
PrioritySetter from it. It also verifies that no votes are leaked.

## Invariants

- **No cycles:** The MaxVoteAggregator CHECKs that the top vote for a node
  doesn't change while its observers are being notified, which catches any
  cycle.
- **Re-entrancy:** Votes can be cast while the MaxVoteAggregator is notifying
  its observers (e.g. the InheritParentPriorityVoter votes on child frames when
  the PrioritySetter sets the priority of a frame), so the aggregator supports
  being re-entered.
- **No leaks:** Voters must remove their votes on a node before it is removed
  from the graph (this is CHECKed).
